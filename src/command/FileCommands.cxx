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
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "ls.hxx"

#include <assert.h>

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
handle_read_comments(Client &client, gcc_unused int argc, char *argv[])
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
		AllocatedPath path_fs = map_uri_fs(uri);
		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "No such file");
			return CommandResult::ERROR;
		}

		return read_file_comments(client, path_fs);
#else
		command_error(client, ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif
	} else {
		command_error(client, ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}
}
