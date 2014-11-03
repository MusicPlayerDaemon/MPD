/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "CommandError.hxx"
#include "protocol/Ack.hxx"
#include "protocol/Result.hxx"
#include "client/Client.hxx"
#include "util/CharUtil.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "tag/TagHandler.hxx"
#include "tag/ApeTag.hxx"
#include "tag/TagId3.hxx"
#include "TagStream.hxx"
#include "TagFile.hxx"
#include "storage/StorageInterface.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/DirectoryReader.hxx"
#include "TimePrint.hxx"
#include "ls.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <inttypes.h> /* for PRIu64 */

gcc_pure
static bool
SkipNameFS(const char *name_fs)
{
	return name_fs[0] == '.' &&
		(name_fs[1] == 0 ||
		 (name_fs[1] == '.' && name_fs[2] == 0));
}

gcc_pure
static bool
skip_path(const char *name_fs)
{
	return strchr(name_fs, '\n') != nullptr;
}

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
/* PRIu64 causes bogus compiler warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

CommandResult
handle_listfiles_local(Client &client, const char *path_utf8)
{
	const auto path_fs = AllocatedPath::FromUTF8(path_utf8);
	if (path_fs.IsNull()) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "unsupported file name");
		return CommandResult::ERROR;
	}

	Error error;
	if (!client.AllowFile(path_fs, error))
		return print_error(client, error);

	DirectoryReader reader(path_fs);
	if (reader.HasFailed()) {
		error.FormatErrno("Failed to open '%s'", path_utf8);
		return print_error(client, error);
	}

	while (reader.ReadEntry()) {
		const Path name_fs = reader.GetEntry();
		if (SkipNameFS(name_fs.c_str()) || skip_path(name_fs.c_str()))
			continue;

		std::string name_utf8 = name_fs.ToUTF8();
		if (name_utf8.empty())
			continue;

		const AllocatedPath full_fs =
			AllocatedPath::Build(path_fs, name_fs);
		struct stat st;
		if (!StatFile(full_fs, st, false))
			continue;

		if (S_ISREG(st.st_mode)) {
			client_printf(client, "file: %s\n"
				      "size: %" PRIu64 "\n",
				      name_utf8.c_str(),
				      uint64_t(st.st_size));
		} else if (S_ISDIR(st.st_mode))
			client_printf(client, "directory: %s\n",
				      name_utf8.c_str());

		time_print(client, "Last-Modified", st.st_mtime);
	}

	return CommandResult::OK;
}

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
#pragma GCC diagnostic pop
#endif

gcc_pure
static bool
IsValidName(const char *p)
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
IsValidValue(const char *p)
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
	Client &client = *(Client *)ctx;

	if (IsValidName(key) && IsValidValue(value))
		client_printf(client, "%s: %s\n", key, value);
}

static constexpr tag_handler print_comment_handler = {
	nullptr,
	nullptr,
	print_pair,
};

static CommandResult
read_stream_comments(Client &client, const char *uri)
{
	if (!uri_supported_scheme(uri)) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "unsupported URI scheme");
		return CommandResult::ERROR;
	}

	if (!tag_stream_scan(uri, print_comment_handler, &client)) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "Failed to load file");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;

}

static CommandResult
read_file_comments(Client &client, const Path path_fs)
{
	if (!tag_file_scan(path_fs, print_comment_handler, &client)) {
		command_error(client, ACK_ERROR_NO_EXIST,
			      "Failed to load file");
		return CommandResult::ERROR;
	}

	tag_ape_scan2(path_fs, &print_comment_handler, &client);
	tag_id3_scan(path_fs, &print_comment_handler, &client);

	return CommandResult::OK;

}

CommandResult
handle_read_comments(Client &client, gcc_unused unsigned argc, char *argv[])
{
	assert(argc == 2);

	const char *const uri = argv[1];

	if (memcmp(uri, "file:///", 8) == 0) {
		/* read comments from arbitrary local file */
		const char *path_utf8 = uri + 7;
		AllocatedPath path_fs = AllocatedPath::FromUTF8(path_utf8);
		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client.AllowFile(path_fs, error))
			return print_error(client, error);

		return read_file_comments(client, path_fs);
	} else if (uri_has_scheme(uri)) {
		return read_stream_comments(client, uri);
	} else if (!PathTraitsUTF8::IsAbsolute(uri)) {
#ifdef ENABLE_DATABASE
		const Storage *storage = client.GetStorage();
		if (storage == nullptr) {
#endif
			command_error(client, ACK_ERROR_NO_EXIST,
				      "No database");
			return CommandResult::ERROR;
#ifdef ENABLE_DATABASE
		}

		{
			AllocatedPath path_fs = storage->MapFS(uri);
			if (!path_fs.IsNull())
				return read_file_comments(client, path_fs);
		}

		{
			const std::string uri2 = storage->MapUTF8(uri);
			if (uri_has_scheme(uri2.c_str()))
				return read_stream_comments(client,
							    uri2.c_str());
		}

		command_error(client, ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
#endif
	} else {
		command_error(client, ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}
}
