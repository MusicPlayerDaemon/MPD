/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define __STDC_FORMAT_MACROS /* for PRIu64 */

#include "config.h"
#include "FileCommands.hxx"
#include "Request.hxx"
#include "CommandError.hxx"
#include "protocol/Ack.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/CharUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/Macros.hxx"
#include "util/StringCompare.hxx"
#include "util/ASCII.hxx"
#include "util/Domain.hxx"
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "TagStream.hxx"
#include "TagFile.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "storage/Registry.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileInfo.hxx"
#include "fs/DirectoryReader.hxx"
#include "input/InputStream.hxx"
#include "LocateUri.hxx"
#include "TimePrint.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "thread/Cond.hxx"
#include "thread/Mutex.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "Log.hxx"
#include "Main.hxx"

#include "thread/Cond.hxx"

#include <assert.h>
#include <inttypes.h> /* for PRIu64 */

static constexpr Domain domain("file_commands");

gcc_pure
static bool
SkipNameFS(PathTraitsFS::const_pointer_type name_fs) noexcept
{
	return name_fs[0] == '.' &&
		(name_fs[1] == 0 ||
		 (name_fs[1] == '.' && name_fs[2] == 0));
}

gcc_pure
static bool
skip_path(Path name_fs) noexcept
{
	return name_fs.HasNewline();
}

gcc_pure
static bool
skip_path(const char *name_utf8)
{
	return strchr(name_utf8, '\n') != nullptr;
}

#if defined(_WIN32) && GCC_CHECK_VERSION(4,6)
/* PRIu64 causes bogus compiler warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

CommandResult
handle_listfiles_local(Response &r, Path path_fs)
{
	DirectoryReader reader(path_fs);

	while (reader.ReadEntry()) {
		const Path name_fs = reader.GetEntry();
		if (SkipNameFS(name_fs.c_str()) || skip_path(name_fs))
			continue;

		std::string name_utf8 = name_fs.ToUTF8();
		if (name_utf8.empty())
			continue;

		const AllocatedPath full_fs =
			AllocatedPath::Build(path_fs, name_fs);
		FileInfo fi;
		if (!GetFileInfo(full_fs, fi, false))
			continue;

		if (fi.IsRegular())
			r.Format("file: %s\n"
				 "size: %" PRIu64 "\n",
				 name_utf8.c_str(),
				 fi.GetSize());
		else if (fi.IsDirectory())
			r.Format("directory: %s\n", name_utf8.c_str());
		else
			continue;

		time_print(r, "Last-Modified", fi.GetModificationTime());
	}

	return CommandResult::OK;
}

#if defined(_WIN32) && GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

gcc_pure
static bool
IsValidName(const char *p) noexcept
{
	if (!IsAlphaASCII(*p))
		return false;

	while (*++p) {
		const char ch = *p;
		if (!IsAlphaASCII(ch) && ch != '_' && ch != '-')
			return false;
	}

	return true;
}

gcc_pure
static bool
IsValidValue(const char *p) noexcept
{
	while (*p) {
		const char ch = *p++;

		if ((unsigned char)ch < 0x20)
			return false;
	}

	return true;
}

static void
print_pair(const char *key, const char *value, void *ctx)
{
	auto &r = *(Response *)ctx;

	if (IsValidName(key) && IsValidValue(value))
		r.Format("%s: %s\n", key, value);
}

static constexpr TagHandler print_comment_handler = {
	nullptr,
	nullptr,
	print_pair,
};

static CommandResult
read_stream_comments(Response &r, const char *uri)
{
	if (!tag_stream_scan(uri, print_comment_handler, &r)) {
		r.Error(ACK_ERROR_NO_EXIST, "Failed to load file");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;

}

static CommandResult
read_file_comments(Response &r, const Path path_fs)
{
	if (!tag_file_scan(path_fs, print_comment_handler, &r)) {
		r.Error(ACK_ERROR_NO_EXIST, "Failed to load file");
		return CommandResult::ERROR;
	}

	ScanGenericTags(path_fs, print_comment_handler, &r);

	return CommandResult::OK;

}

static CommandResult
read_db_comments(Client &client, Response &r, const char *uri)
{
#ifdef ENABLE_DATABASE
	const Storage *storage = client.GetStorage();
	if (storage == nullptr) {
#else
		(void)client;
		(void)uri;
#endif
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#ifdef ENABLE_DATABASE
	}

	{
		AllocatedPath path_fs = storage->MapFS(uri);
		if (!path_fs.IsNull())
			return read_file_comments(r, path_fs);
	}

	{
		const std::string uri2 = storage->MapUTF8(uri);
		if (uri_has_scheme(uri2.c_str()))
			return read_stream_comments(r, uri2.c_str());
	}

	r.Error(ACK_ERROR_NO_EXIST, "No such file");
	return CommandResult::ERROR;
#endif
}

CommandResult
handle_read_comments(Client &client, Request args, Response &r)
{
	assert(args.size == 1);

	const char *const uri = args.front();

	const auto located_uri = LocateUri(uri, &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return read_stream_comments(r, located_uri.canonical_uri);

	case LocatedUri::Type::RELATIVE:
		return read_db_comments(client, r, located_uri.canonical_uri);

	case LocatedUri::Type::PATH:
		return read_file_comments(r, located_uri.path);
	}

	gcc_unreachable();
}

/**
 * Searches for the files listed in #artnames in the UTF8 folder
 * URI #directory. This can be a local path or protocol-based
 * URI that #InputStream supports. Returns the first successfully
 * opened file or #nullptr on failure.
 */
static InputStreamPtr
find_stream_art(const char *directory, Mutex &mutex, Cond &cond)
{
	static constexpr char const * art_names[] = {
		"cover.png",
		"cover.jpg",
		"cover.tiff",
		"cover.bmp"
	};

	for(const auto name: art_names) {
		std::string art_file = PathTraitsUTF8::Build(directory, name);

		try {
			return InputStream::OpenReady(art_file.c_str(), mutex, cond);
		} catch (const std::exception &e) {}
	}
	return nullptr;
}

static CommandResult
read_stream_art(Response &r, const char *uri, size_t offset)
{
	std::string art_directory = PathTraitsUTF8::GetParent(uri);

	Mutex mutex;
	Cond cond;

	InputStreamPtr is = find_stream_art(art_directory.c_str(), mutex, cond);

	if (is == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No file exists");
		return CommandResult::ERROR;
	}
	if (!is->KnownSize()) {
		r.Error(ACK_ERROR_NO_EXIST, "Cannot get size for stream");
		return CommandResult::ERROR;
	}

	const size_t art_file_size = is->GetSize();

	constexpr size_t CHUNK_SIZE = 8192;
	uint8_t buffer[CHUNK_SIZE];
	size_t read_size;

	is->Seek(offset);
	read_size = is->Read(&buffer, CHUNK_SIZE);

	r.Format("size: %" PRIu64 "\n"
			 "binary: %u\n",
			 art_file_size,
			 read_size
			 );

	r.Write(buffer, read_size);
	r.Write("\n");

	return CommandResult::OK;
}

#ifdef ENABLE_DATABASE
static CommandResult
read_db_art(Client &client, Response &r, const char *uri, const uint64_t offset)
{
	const Storage *storage = client.GetStorage();
	if (storage == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
	}
	std::string uri2 = storage->MapUTF8(uri);
	return read_stream_art(r, uri2.c_str(), offset);
}
#endif

CommandResult
handle_album_art(Client &client, Request args, Response &r)
{
	assert(args.size == 2);

	const char *uri = args.front();
	size_t offset = args.ParseUnsigned(1);

	const auto located_uri = LocateUri(uri, &client
#ifdef ENABLE_DATABASE
					   , nullptr
#endif
					   );

	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
	case LocatedUri::Type::PATH:
		return read_stream_art(r, located_uri.canonical_uri, offset);
	case LocatedUri::Type::RELATIVE:
#ifdef ENABLE_DATABASE
		return read_db_art(client, r, located_uri.canonical_uri, offset);
#else
		r.Error(ACK_ERROR_NO_EXIST, "Database disabled");
		return CommandResult::ERROR;
#endif
	}
	r.Error(ACK_ERROR_NO_EXIST, "No art file exists");
	return CommandResult::ERROR;
}

static int cover_count = 0;
static inline void inc_cover_count()
{
	cover_count++;
}
static inline void clear_cover_cnt()
{
	cover_count = 0;
}
static inline int get_cover_cnt()
{
	return cover_count;
}

static void
print_cover(CoverType type, const char *value, void *ctx, size_t length=0)
{
	auto &r = *(Response *)ctx;

	if (get_cover_cnt() > 0) {
		return;
	}
	if (length == 0) {
		r.Format("%s: %s\n", cover_item_names[type], value);
	} else {
		r.Format("%s: \n", cover_item_names[type]);
		r.Write(value, length);
		r.Format("\n");
		inc_cover_count();
	}
}

static constexpr TagHandler print_cover_handler = {
	nullptr,
	nullptr,
	nullptr,
	print_cover,
};

static bool
read_db_song_cover(Client &client, Response &r, const char *uri)
{
#ifdef ENABLE_DATABASE
	const Storage *storage = client.GetStorage();
	if (storage == nullptr) {
#else
		(void)client;
		(void)uri;
#endif
		return false;
#ifdef ENABLE_DATABASE
	}

	{
		AllocatedPath path_fs = storage->MapFS(uri);
		if (!path_fs.IsNull())
			return tag_file_scan(path_fs, print_cover_handler, &r);
	}

	{
		const std::string uri2 = storage->MapUTF8(uri);
		if (uri_has_scheme(uri2.c_str()))
			return tag_stream_scan(uri2.c_str(), print_cover_handler, &r);
	}

	return false;
#endif
}

static bool
handle_read_song_cover(Client &client, Request args, Response &r)
{
	assert(args.size == 1);

	const char *const uri = args.front();
	r.Format("file: %s\n", uri);

	const auto located_uri = LocateUri(uri, &client,
#ifdef ENABLE_DATABASE
					   nullptr
#endif
					   );
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return tag_stream_scan(located_uri.canonical_uri, print_cover_handler, &r);

	case LocatedUri::Type::RELATIVE:
		return read_db_song_cover(client, r, located_uri.canonical_uri);

	case LocatedUri::Type::PATH:
		return tag_file_scan(located_uri.path, print_cover_handler, &r);
	}

	gcc_unreachable();
}

static bool
cover_scan_stream(InputStream &is, const TagHandler &handler, void *handler_ctx)
{
	size_t size = is.GetSize();
	if (size == 0)
		return false;
	size_t pos = 0;
	char *buf = new char[size];
	while (size > pos) {
		size_t nbytes = is.Read(buf+pos , size-pos);
		if (nbytes == 0 && is.IsEOF()) {
			break;
		}
		pos += nbytes;
	}
	if (size != pos) {
		delete[] buf;
		return false;
	}
	char length[20];
	snprintf(length, sizeof(length), "%u", size);
	tag_handler_invoke_cover(handler, handler_ctx, COVER_LENGTH, (const char*)length);
	tag_handler_invoke_cover(handler, handler_ctx, COVER_DATA, buf, size);
	delete[] buf;

	return true;
}

struct FolderInfo
{
	std::string filename;
	StorageFileInfo::Type type;
};

gcc_pure gcc_nonnull_all
static inline bool
StringCaseStartsWith(const char *haystack, StringView needle) noexcept
{
	return strncasecmp(haystack, needle.data, needle.size) == 0;
}

static bool
StringCaseEndsWith(const char *haystack, const char *needle) noexcept
{
	const size_t haystack_length = strlen(haystack);
	const size_t needle_length = strlen(needle);
	return haystack_length >= needle_length &&
		strncasecmp(haystack+(haystack_length-needle_length), needle, needle_length) == 0;
}

static std::vector<FolderInfo>
list_image_file_info(Storage &storage, const char *uri)
{
	std::vector<FolderInfo> list;

	std::unique_ptr<StorageDirectoryReader> reader(storage.OpenDirectory(uri));
	if (reader == nullptr)
		return list;

	const char *name_utf8;
	while ((name_utf8 = reader->Read()) != nullptr) {
		if (skip_path(name_utf8))
			continue;

		try {
			StorageFileInfo info = reader->GetInfo(false);
			switch (info.type) {
			case StorageFileInfo::Type::OTHER:
				/* ignore */
				continue;

			case StorageFileInfo::Type::REGULAR:
				if (StringCaseEndsWith(name_utf8, ".jpg") ||
					StringCaseEndsWith(name_utf8, ".jpeg") ||
					StringCaseEndsWith(name_utf8, ".png") ||
					StringCaseEndsWith(name_utf8, ".bmp") ||
					StringCaseEndsWith(name_utf8, ".tiff")) {
					list.push_back({name_utf8, StorageFileInfo::Type::REGULAR});
				}
				break;

			case StorageFileInfo::Type::DIRECTORY:
				list.push_back({name_utf8, StorageFileInfo::Type::DIRECTORY});
				break;
			}
		} catch (...) {
			continue;
		}

	}

	return list;
}

static bool
read_stream_folder_cover(Response &r, const char *uri)
{
	Mutex mutex;
	Cond cond;
	bool ret = false;
	auto &event_loop = instance->io_thread.GetEventLoop();

	std::unique_ptr<Storage> storage(CreateStorageURI(event_loop, uri));
	if (storage == nullptr) {
		return false;
	}

	auto list = list_image_file_info(*storage, "");

	static const std::vector<std::string> table = {
		"cover",
		"front",
		"folder",
		"back",
	};

	for (const auto &it : table) {
		for (const auto &info : list) {
			if (StringCaseStartsWith(info.filename.c_str(), it.c_str())) {
				std::string cover_uri = uri;
				if (cover_uri.back() != '/')
					cover_uri += "/";
				cover_uri += info.filename;
				try {
					auto is = InputStream::OpenReady(cover_uri.c_str(), mutex, cond);
					if (is == nullptr) {
						continue;
					}
					ret = cover_scan_stream(*is, print_cover_handler, &r);
					if (ret) {
						return true;
					}
				} catch (...) {
				}
			}
		}
	}

	return false;
}

static bool
read_db_folder_cover(Client &client, Response &r, const char *uri)
{
#ifdef ENABLE_DATABASE
	const Storage *storage = client.GetStorage();
	if (storage == nullptr) {
#else
		(void)client;
		(void)uri;
#endif
		return false;
#ifdef ENABLE_DATABASE
	}

	{
		AllocatedPath path_fs = storage->MapFS(uri);
		if (path_fs.IsNull())
			return false;
	}

	const std::string uri2 = storage->MapUTF8(uri);
	return read_stream_folder_cover(r, uri2.c_str());

#endif
}

static bool
handle_read_folder_cover(Client &client, Request args, Response &r)
{
	assert(args.size == 1);

	const char *const uri = args.front();

	r.Format("folder: %s\n", uri);
	const auto located_uri = LocateUri(uri, &client,
#ifdef ENABLE_DATABASE
					   nullptr
#endif
					   );
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return read_stream_folder_cover(r, located_uri.canonical_uri);

	case LocatedUri::Type::RELATIVE:
		return read_db_folder_cover(client, r, located_uri.canonical_uri);

	case LocatedUri::Type::PATH:
		return false;
	}

	gcc_unreachable();
}

CommandResult
handle_read_cover(Client &client, Request args, Response &r)
{
	const char *const uri = args.front();

	if (StringStartsWith(uri, "https://api.tidalhifi.com") ||
		StringStartsWith(uri, "https://api.tidal.com") ||
		StringCaseEndsWith(uri, ".dat") ||
		StringCaseEndsWith(uri, ".iso") ||
		StringFind(uri, ".vtuner.com") != nullptr) {
		r.Format("file: %s\n", uri);
		return CommandResult::OK;
	}
	clear_cover_cnt();
	if (handle_read_song_cover(client, args, r) &&
		get_cover_cnt() > 0) {
		FormatDefault(domain, "find cover in the song");
		return CommandResult::OK;
	}

	if (uri_has_scheme(uri)) {
		return CommandResult::OK;
	}
	const char *argv[1];
	Request args2(argv, 0);
	std::string str = args.front();
	size_t pos = str.rfind("/");
	if (pos == std::string::npos) {
		return CommandResult::OK;
	}
	str = str.substr(0, pos);
	if (str.empty()) {
		return CommandResult::OK;
	}
	argv[args2.size++] = str.c_str();
	auto ret = handle_read_folder_cover(client, args2, r);
	if (ret) {
		FormatDefault(domain, "find cover in the folder");
	}
	return CommandResult::OK;
}
