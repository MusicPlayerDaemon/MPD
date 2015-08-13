/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "util/ConstBuffer.hxx"
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
#include "fs/FileInfo.hxx"
#include "fs/DirectoryReader.hxx"
#include "TimePrint.hxx"
#include "ls.hxx"

#include <assert.h>
#include <sys/stat.h>
#include <inttypes.h> /* for PRIu64 */

gcc_pure
static bool
SkipNameFS(PathTraitsFS::const_pointer name_fs)
{
	return name_fs[0] == '.' &&
		(name_fs[1] == 0 ||
		 (name_fs[1] == '.' && name_fs[2] == 0));
}

gcc_pure
static bool
skip_path(Path name_fs)
{
	return name_fs.HasNewline();
}

#if defined(WIN32) && GCC_CHECK_VERSION(4,6)
/* PRIu64 causes bogus compiler warning */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#endif

CommandResult
handle_listfiles_local(Client &client, Response &r,
		       const char *path_utf8)
{
	const auto path_fs = AllocatedPath::FromUTF8(path_utf8);
	if (path_fs.IsNull()) {
		r.Error(ACK_ERROR_NO_EXIST, "unsupported file name");
		return CommandResult::ERROR;
	}

	Error error;
	if (!client.AllowFile(path_fs, error))
		return print_error(r, error);

	DirectoryReader reader(path_fs);
	if (reader.HasFailed()) {
		error.FormatErrno("Failed to open '%s'", path_utf8);
		return print_error(r, error);
	}

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
	auto &r = *(Response *)ctx;

	if (IsValidName(key) && IsValidValue(value))
		r.Format("%s: %s\n", key, value);
}

static constexpr tag_handler print_comment_handler = {
	nullptr,
	nullptr,
	print_pair,
};

static CommandResult
read_stream_comments(Response &r, const char *uri)
{
	if (!uri_supported_scheme(uri)) {
		r.Error(ACK_ERROR_NO_EXIST, "unsupported URI scheme");
		return CommandResult::ERROR;
	}

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

	tag_ape_scan2(path_fs, &print_comment_handler, &r);
	tag_id3_scan(path_fs, &print_comment_handler, &r);

	return CommandResult::OK;

}

static const char *
translate_uri(const char *uri)
{
	if (memcmp(uri, "file:///", 8) == 0)
		/* drop the "file://", leave only an absolute path
		   (starting with a slash) */
		return uri + 7;

	return uri;
}

CommandResult
handle_read_comments(Client &client, Request args, Response &r)
{
	assert(args.size == 1);
	const char *const uri = translate_uri(args.front());

	if (memcmp(uri, "file:///", 8) == 0) {
		/* read comments from arbitrary local file */
		const char *path_utf8 = uri + 7;
		AllocatedPath path_fs = AllocatedPath::FromUTF8(path_utf8);
		if (path_fs.IsNull()) {
			r.Error(ACK_ERROR_NO_EXIST, "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client.AllowFile(path_fs, error))
			return print_error(r, error);

		return read_file_comments(r, path_fs);
	} else if (uri_has_scheme(uri)) {
		return read_stream_comments(r, uri);
	} else if (!PathTraitsUTF8::IsAbsolute(uri)) {
#ifdef ENABLE_DATABASE
		const Storage *storage = client.GetStorage();
		if (storage == nullptr) {
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
	} else {
		r.Error(ACK_ERROR_NO_EXIST, "No such file");
		return CommandResult::ERROR;
	}
}
