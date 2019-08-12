/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "protocol/Ack.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "util/CharUtil.hxx"
#include "util/OffsetPointer.hxx"
#include "util/StringView.hxx"
#include "util/UriExtract.hxx"
#include "tag/Handler.hxx"
#include "tag/Generic.hxx"
#include "TagAny.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileInfo.hxx"
#include "fs/DirectoryReader.hxx"
#include "input/InputStream.hxx"
#include "input/Error.hxx"
#include "LocateUri.hxx"
#include "TimePrint.hxx"
#include "thread/Mutex.hxx"
#include "Log.hxx"

#include <assert.h>
#include <inttypes.h> /* for PRIu64 */

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

		const auto full_fs = path_fs / name_fs;
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
IsValidName(const StringView s) noexcept
{
	if (s.empty() || !IsAlphaASCII(s.front()))
		return false;

	for (const char ch : s) {
		if (!IsAlphaASCII(ch) && ch != '_' && ch != '-')
			return false;
	}

	return true;
}

gcc_pure
static bool
IsValidValue(const StringView s) noexcept
{
	for (const char ch : s) {
		if ((unsigned char)ch < 0x20)
			return false;
	}

	return true;
}

class PrintCommentHandler final : public NullTagHandler {
	Response &response;

public:
	explicit PrintCommentHandler(Response &_response) noexcept
		:NullTagHandler(WANT_PAIR), response(_response) {}

	void OnPair(StringView key, StringView value) noexcept override {
		if (IsValidName(key) && IsValidValue(value))
			response.Format("%.*s: %.*s\n",
					int(key.size), key.data,
					int(value.size), value.data);
	}
};

CommandResult
handle_read_comments(Client &client, Request args, Response &r)
{
	assert(args.size == 1);

	const char *const uri = args.front();

	PrintCommentHandler handler(r);
	TagScanAny(client, uri, handler);
	return CommandResult::OK;
}

/**
 * Searches for the files listed in #artnames in the UTF8 folder
 * URI #directory. This can be a local path or protocol-based
 * URI that #InputStream supports. Returns the first successfully
 * opened file or #nullptr on failure.
 */
static InputStreamPtr
find_stream_art(const char *directory, Mutex &mutex)
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
			return InputStream::OpenReady(art_file.c_str(), mutex);
		} catch (...) {
			auto e = std::current_exception();
			if (!IsFileNotFound(e))
				LogError(e);
		}
	}
	return nullptr;
}

static CommandResult
read_stream_art(Response &r, const char *uri, size_t offset)
{
	std::string art_directory = PathTraitsUTF8::GetParent(uri);

	Mutex mutex;

	InputStreamPtr is = find_stream_art(art_directory.c_str(), mutex);

	if (is == nullptr) {
		r.Error(ACK_ERROR_NO_EXIST, "No file exists");
		return CommandResult::ERROR;
	}
	if (!is->KnownSize()) {
		r.Error(ACK_ERROR_NO_EXIST, "Cannot get size for stream");
		return CommandResult::ERROR;
	}

	const offset_type art_file_size = is->GetSize();

	uint8_t buffer[Response::MAX_BINARY_SIZE];
	size_t read_size;

	{
		std::unique_lock<Mutex> lock(mutex);
		is->Seek(lock, offset);
		read_size = is->Read(lock, &buffer, sizeof(buffer));
	}

	r.Format("size: %" PRIoffset "\n", art_file_size);
	r.WriteBinary({buffer, read_size});

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

	const auto located_uri = LocateUri(UriPluginKind::INPUT, uri, &client
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

class PrintPictureHandler final : public NullTagHandler {
	Response &response;

	const size_t offset;

	bool found = false;

	bool bad_offset = false;

public:
	PrintPictureHandler(Response &_response, size_t _offset) noexcept
		:NullTagHandler(WANT_PICTURE), response(_response),
		 offset(_offset) {}

	void RethrowError() const {
		if (bad_offset)
			throw ProtocolError(ACK_ERROR_ARG, "Bad file offset");
	}

	void OnPicture(const char *mime_type,
		       ConstBuffer<void> buffer) noexcept override {
		if (found)
			/* only use the first picture */
			return;

		found = true;

		if (offset > buffer.size) {
			bad_offset = true;
			return;
		}

		response.Format("size: %" PRIoffset "\n", buffer.size);

		if (mime_type != nullptr)
			response.Format("type: %s\n", mime_type);

		buffer.size -= offset;
		if (buffer.size > Response::MAX_BINARY_SIZE)
			buffer.size = Response::MAX_BINARY_SIZE;
		buffer.data = OffsetPointer(buffer.data, offset);

		response.WriteBinary(buffer);
	}
};

CommandResult
handle_read_picture(Client &client, Request args, Response &r)
{
	assert(args.size == 2);

	const char *const uri = args.front();
	const size_t offset = args.ParseUnsigned(1);

	PrintPictureHandler handler(r, offset);
	TagScanAny(client, uri, handler);
	handler.RethrowError();
	return CommandResult::OK;
}
