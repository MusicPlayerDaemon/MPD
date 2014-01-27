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
#include "OtherCommands.hxx"
#include "DatabaseCommands.hxx"
#include "db/update/UpdateGlue.hxx"
#include "CommandError.hxx"
#include "db/Directory.hxx"
#include "DetachedSong.hxx"
#include "SongPrint.hxx"
#include "TagPrint.hxx"
#include "TagStream.hxx"
#include "tag/TagHandler.hxx"
#include "TimePrint.hxx"
#include "Mapper.hxx"
#include "decoder/DecoderPrint.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"
#include "mixer/Volume.hxx"
#include "util/ASCII.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"
#include "Stats.hxx"
#include "Permission.hxx"
#include "PlaylistFile.hxx"
#include "db/PlaylistVector.hxx"
#include "client/ClientFile.hxx"
#include "client/Client.hxx"
#include "Idle.hxx"

#include <assert.h>
#include <string.h>

static void
print_spl_list(Client &client, const PlaylistVector &list)
{
	for (const auto &i : list) {
		client_printf(client, "playlist: %s\n", i.name.c_str());

		if (i.mtime > 0)
			time_print(client, "Last-Modified", i.mtime);
	}
}

CommandResult
handle_urlhandlers(Client &client,
		   gcc_unused int argc, gcc_unused char *argv[])
{
	if (client.IsLocal())
		client_puts(client, "handler: file://\n");
	print_supported_uri_schemes(client);
	return CommandResult::OK;
}

CommandResult
handle_decoders(Client &client,
		gcc_unused int argc, gcc_unused char *argv[])
{
	decoder_list_print(client);
	return CommandResult::OK;
}

CommandResult
handle_tagtypes(Client &client,
		gcc_unused int argc, gcc_unused char *argv[])
{
	tag_print_types(client);
	return CommandResult::OK;
}

CommandResult
handle_kill(gcc_unused Client &client,
	    gcc_unused int argc, gcc_unused char *argv[])
{
	return CommandResult::KILL;
}

CommandResult
handle_close(gcc_unused Client &client,
	     gcc_unused int argc, gcc_unused char *argv[])
{
	return CommandResult::FINISH;
}

static void
print_tag(TagType type, const char *value, void *ctx)
{
	Client &client = *(Client *)ctx;

	tag_print(client, type, value);
}

static constexpr tag_handler print_tag_handler = {
	nullptr,
	print_tag,
	nullptr,
};

CommandResult
handle_lsinfo(Client &client, int argc, char *argv[])
{
	const char *uri;

	if (argc == 2)
		uri = argv[1];
	else
		/* default is root directory */
		uri = "";

	if (memcmp(uri, "file:///", 8) == 0) {
		/* print information about an arbitrary local file */
		const char *path_utf8 = uri + 7;
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8);

		if (path_fs.IsNull()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported file name");
			return CommandResult::ERROR;
		}

		Error error;
		if (!client_allow_file(client, path_fs, error))
			return print_error(client, error);

		DetachedSong song(path_utf8);
		if (!song.Update()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "No such file");
			return CommandResult::ERROR;
		}

		song_print_info(client, song);
		return CommandResult::OK;
	}

	if (uri_has_scheme(uri)) {
		if (!uri_supported_scheme(uri)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "unsupported URI scheme");
			return CommandResult::ERROR;
		}

		if (!tag_stream_scan(uri, print_tag_handler, &client)) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "No such file");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	}

	CommandResult result = handle_lsinfo2(client, argc, argv);
	if (result != CommandResult::OK)
		return result;

	if (isRootDirectory(uri)) {
		Error error;
		const auto &list = ListPlaylistFiles(error);
		print_spl_list(client, list);
	}

	return CommandResult::OK;
}

CommandResult
handle_update(Client &client, gcc_unused int argc, char *argv[])
{
	const char *path = "";
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2) {
		path = argv[1];

		if (*path == 0 || strcmp(path, "/") == 0)
			/* backwards compatibility with MPD 0.15 */
			path = "";
		else if (!uri_safe_local(path)) {
			command_error(client, ACK_ERROR_ARG,
				      "Malformed path");
			return CommandResult::ERROR;
		}
	}

	ret = update_enqueue(path, false);
	if (ret > 0) {
		client_printf(client, "updating_db: %i\n", ret);
		return CommandResult::OK;
	} else {
		command_error(client, ACK_ERROR_UPDATE_ALREADY,
			      "already updating");
		return CommandResult::ERROR;
	}
}

CommandResult
handle_rescan(Client &client, gcc_unused int argc, char *argv[])
{
	const char *path = "";
	unsigned ret;

	assert(argc <= 2);
	if (argc == 2) {
		path = argv[1];

		if (!uri_safe_local(path)) {
			command_error(client, ACK_ERROR_ARG,
				      "Malformed path");
			return CommandResult::ERROR;
		}
	}

	ret = update_enqueue(path, true);
	if (ret > 0) {
		client_printf(client, "updating_db: %i\n", ret);
		return CommandResult::OK;
	} else {
		command_error(client, ACK_ERROR_UPDATE_ALREADY,
			      "already updating");
		return CommandResult::ERROR;
	}
}

CommandResult
handle_setvol(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned level;
	bool success;

	if (!check_unsigned(client, &level, argv[1]))
		return CommandResult::ERROR;

	if (level > 100) {
		command_error(client, ACK_ERROR_ARG, "Invalid volume value");
		return CommandResult::ERROR;
	}

	success = volume_level_change(level);
	if (!success) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_volume(Client &client, gcc_unused int argc, char *argv[])
{
	int relative;
	if (!check_int(client, &relative, argv[1]))
		return CommandResult::ERROR;

	if (relative < -100 || relative > 100) {
		command_error(client, ACK_ERROR_ARG, "Invalid volume value");
		return CommandResult::ERROR;
	}

	const int old_volume = volume_level_get();
	if (old_volume < 0) {
		command_error(client, ACK_ERROR_SYSTEM, "No mixer");
		return CommandResult::ERROR;
	}

	int new_volume = old_volume + relative;
	if (new_volume < 0)
		new_volume = 0;
	else if (new_volume > 100)
		new_volume = 100;

	if (new_volume != old_volume && !volume_level_change(new_volume)) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_stats(Client &client,
	     gcc_unused int argc, gcc_unused char *argv[])
{
	stats_print(client);
	return CommandResult::OK;
}

CommandResult
handle_ping(gcc_unused Client &client,
	    gcc_unused int argc, gcc_unused char *argv[])
{
	return CommandResult::OK;
}

CommandResult
handle_password(Client &client, gcc_unused int argc, char *argv[])
{
	unsigned permission = 0;

	if (getPermissionFromPassword(argv[1], &permission) < 0) {
		command_error(client, ACK_ERROR_PASSWORD, "incorrect password");
		return CommandResult::ERROR;
	}

	client.SetPermission(permission);

	return CommandResult::OK;
}

CommandResult
handle_config(Client &client,
	      gcc_unused int argc, gcc_unused char *argv[])
{
	if (!client.IsLocal()) {
		command_error(client, ACK_ERROR_PERMISSION,
			      "Command only permitted to local clients");
		return CommandResult::ERROR;
	}

	const char *path = mapper_get_music_directory_utf8();
	if (path != nullptr)
		client_printf(client, "music_directory: %s\n", path);

	return CommandResult::OK;
}

CommandResult
handle_idle(Client &client,
	    gcc_unused int argc, gcc_unused char *argv[])
{
	unsigned flags = 0, j;
	int i;
	const char *const* idle_names;

	idle_names = idle_get_names();
	for (i = 1; i < argc; ++i) {
		if (!argv[i])
			continue;

		for (j = 0; idle_names[j]; ++j) {
			if (StringEqualsCaseASCII(argv[i], idle_names[j])) {
				flags |= (1 << j);
			}
		}
	}

	/* No argument means that the client wants to receive everything */
	if (flags == 0)
		flags = ~0;

	/* enable "idle" mode on this client */
	client.IdleWait(flags);

	return CommandResult::IDLE;
}
