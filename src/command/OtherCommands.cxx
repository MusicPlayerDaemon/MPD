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
#include "FileCommands.hxx"
#include "StorageCommands.hxx"
#include "CommandError.hxx"
#include "db/Uri.hxx"
#include "storage/StorageInterface.hxx"
#include "DetachedSong.hxx"
#include "SongPrint.hxx"
#include "TagPrint.hxx"
#include "TagStream.hxx"
#include "tag/TagHandler.hxx"
#include "TimePrint.hxx"
#include "decoder/DecoderPrint.hxx"
#include "protocol/ArgParser.hxx"
#include "protocol/Result.hxx"
#include "ls.hxx"
#include "mixer/Volume.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "fs/AllocatedPath.hxx"
#include "Stats.hxx"
#include "Permission.hxx"
#include "PlaylistFile.hxx"
#include "db/PlaylistVector.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "Idle.hxx"

#ifdef ENABLE_DATABASE
#include "DatabaseCommands.hxx"
#include "db/Interface.hxx"
#include "db/update/Service.hxx"
#endif

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
		   gcc_unused unsigned argc, gcc_unused char *argv[])
{
	if (client.IsLocal())
		client_puts(client, "handler: file://\n");
	print_supported_uri_schemes(client);
	return CommandResult::OK;
}

CommandResult
handle_decoders(Client &client,
		gcc_unused unsigned argc, gcc_unused char *argv[])
{
	decoder_list_print(client);
	return CommandResult::OK;
}

CommandResult
handle_tagtypes(Client &client,
		gcc_unused unsigned argc, gcc_unused char *argv[])
{
	tag_print_types(client);
	return CommandResult::OK;
}

CommandResult
handle_kill(gcc_unused Client &client,
	    gcc_unused unsigned argc, gcc_unused char *argv[])
{
	return CommandResult::KILL;
}

CommandResult
handle_close(gcc_unused Client &client,
	     gcc_unused unsigned argc, gcc_unused char *argv[])
{
	return CommandResult::FINISH;
}

static void
print_tag(TagType type, const char *value, void *ctx)
{
	Client &client = *(Client *)ctx;

	tag_print(client, type, value);
}

CommandResult
handle_listfiles(Client &client, unsigned argc, char *argv[])
{
	const char *const uri = argc == 2
		? argv[1]
		/* default is root directory */
		: "";

	if (memcmp(uri, "file:///", 8) == 0)
		/* list local directory */
		return handle_listfiles_local(client, uri + 7);

#ifdef ENABLE_DATABASE
	if (uri_has_scheme(uri))
		/* use storage plugin to list remote directory */
		return handle_listfiles_storage(client, uri);

	/* must be a path relative to the configured
	   music_directory */

	if (client.partition.instance.storage != nullptr)
		/* if we have a storage instance, obtain a list of
		   files from it */
		return handle_listfiles_storage(client,
						*client.partition.instance.storage,
						uri);

	/* fall back to entries from database if we have no storage */
	return handle_listfiles_db(client, uri);
#else
	command_error(client, ACK_ERROR_NO_EXIST, "No database");
	return CommandResult::ERROR;
#endif
}

static constexpr tag_handler print_tag_handler = {
	nullptr,
	print_tag,
	nullptr,
};

CommandResult
handle_lsinfo(Client &client, unsigned argc, char *argv[])
{
	const char *const uri = argc == 2
		? argv[1]
		/* default is root directory */
		: "";

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
		if (!client.AllowFile(path_fs, error))
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

#ifdef ENABLE_DATABASE
	CommandResult result = handle_lsinfo2(client, argc, argv);
	if (result != CommandResult::OK)
		return result;
#endif

	if (isRootDirectory(uri)) {
		Error error;
		const auto &list = ListPlaylistFiles(error);
		print_spl_list(client, list);
	} else {
#ifndef ENABLE_DATABASE
		command_error(client, ACK_ERROR_NO_EXIST, "No database");
		return CommandResult::ERROR;
#endif
	}

	return CommandResult::OK;
}

#ifdef ENABLE_DATABASE

static CommandResult
handle_update(Client &client, UpdateService &update,
	      const char *uri_utf8, bool discard)
{
	unsigned ret = update.Enqueue(uri_utf8, discard);
	if (ret > 0) {
		client_printf(client, "updating_db: %i\n", ret);
		return CommandResult::OK;
	} else {
		command_error(client, ACK_ERROR_UPDATE_ALREADY,
			      "already updating");
		return CommandResult::ERROR;
	}
}

static CommandResult
handle_update(Client &client, Database &db,
	      const char *uri_utf8, bool discard)
{
	Error error;
	unsigned id = db.Update(uri_utf8, discard, error);
	if (id > 0) {
		client_printf(client, "updating_db: %i\n", id);
		return CommandResult::OK;
	} else if (error.IsDefined()) {
		return print_error(client, error);
	} else {
		/* Database::Update() has returned 0 without setting
		   the Error: the method is not implemented */
		command_error(client, ACK_ERROR_NO_EXIST, "Not implemented");
		return CommandResult::ERROR;
	}
}

#endif

static CommandResult
handle_update(Client &client, unsigned argc, char *argv[], bool discard)
{
#ifdef ENABLE_DATABASE
	const char *path = "";

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

	UpdateService *update = client.partition.instance.update;
	if (update != nullptr)
		return handle_update(client, *update, path, discard);

	Database *db = client.partition.instance.database;
	if (db != nullptr)
		return handle_update(client, *db, path, discard);
#else
	(void)argc;
	(void)argv;
	(void)discard;
#endif

	command_error(client, ACK_ERROR_NO_EXIST, "No database");
	return CommandResult::ERROR;
}

CommandResult
handle_update(Client &client, gcc_unused unsigned argc, char *argv[])
{
	return handle_update(client, argc, argv, false);
}

CommandResult
handle_rescan(Client &client, gcc_unused unsigned argc, char *argv[])
{
	return handle_update(client, argc, argv, true);
}

CommandResult
handle_setvol(Client &client, gcc_unused unsigned argc, char *argv[])
{
	unsigned level;
	bool success;

	if (!check_unsigned(client, &level, argv[1]))
		return CommandResult::ERROR;

	if (level > 100) {
		command_error(client, ACK_ERROR_ARG, "Invalid volume value");
		return CommandResult::ERROR;
	}

	success = volume_level_change(client.partition.outputs, level);
	if (!success) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_volume(Client &client, gcc_unused unsigned argc, char *argv[])
{
	int relative;
	if (!check_int(client, &relative, argv[1]))
		return CommandResult::ERROR;

	if (relative < -100 || relative > 100) {
		command_error(client, ACK_ERROR_ARG, "Invalid volume value");
		return CommandResult::ERROR;
	}

	const int old_volume = volume_level_get(client.partition.outputs);
	if (old_volume < 0) {
		command_error(client, ACK_ERROR_SYSTEM, "No mixer");
		return CommandResult::ERROR;
	}

	int new_volume = old_volume + relative;
	if (new_volume < 0)
		new_volume = 0;
	else if (new_volume > 100)
		new_volume = 100;

	if (new_volume != old_volume &&
	    !volume_level_change(client.partition.outputs, new_volume)) {
		command_error(client, ACK_ERROR_SYSTEM,
			      "problems setting volume");
		return CommandResult::ERROR;
	}

	return CommandResult::OK;
}

CommandResult
handle_stats(Client &client,
	     gcc_unused unsigned argc, gcc_unused char *argv[])
{
	stats_print(client);
	return CommandResult::OK;
}

CommandResult
handle_ping(gcc_unused Client &client,
	    gcc_unused unsigned argc, gcc_unused char *argv[])
{
	return CommandResult::OK;
}

CommandResult
handle_password(Client &client, gcc_unused unsigned argc, char *argv[])
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
	      gcc_unused unsigned argc, gcc_unused char *argv[])
{
	if (!client.IsLocal()) {
		command_error(client, ACK_ERROR_PERMISSION,
			      "Command only permitted to local clients");
		return CommandResult::ERROR;
	}

#ifdef ENABLE_DATABASE
	const Storage *storage = client.GetStorage();
	if (storage != nullptr) {
		const auto path = storage->MapUTF8("");
		client_printf(client, "music_directory: %s\n", path.c_str());
	}
#endif

	return CommandResult::OK;
}

CommandResult
handle_idle(Client &client,
	    gcc_unused unsigned argc, gcc_unused char *argv[])
{
	unsigned flags = 0;

	for (unsigned i = 1; i < argc; ++i) {
		unsigned event = idle_parse_name(argv[i]);
		if (event == 0) {
			command_error(client, ACK_ERROR_ARG,
				      "Unrecognized idle event: %s",
				      argv[i]);
			return CommandResult::ERROR;
		}

		flags |= event;
	}

	/* No argument means that the client wants to receive everything */
	if (flags == 0)
		flags = ~0;

	/* enable "idle" mode on this client */
	client.IdleWait(flags);

	return CommandResult::IDLE;
}
