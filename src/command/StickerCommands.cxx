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
#include "StickerCommands.hxx"
#include "SongPrint.hxx"
#include "db/Interface.hxx"
#include "db/DatabaseGlue.hxx"
#include "sticker/SongSticker.hxx"
#include "sticker/StickerPrint.hxx"
#include "sticker/StickerDatabase.hxx"
#include "CommandError.hxx"
#include "protocol/Result.hxx"
#include "client/Client.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "util/Error.hxx"

#include <string.h>

struct sticker_song_find_data {
	Client &client;
	const char *name;
};

static void
sticker_song_find_print_cb(const LightSong &song, const char *value,
			   void *user_data)
{
	struct sticker_song_find_data *data =
		(struct sticker_song_find_data *)user_data;

	song_print_uri(data->client, song);
	sticker_print_value(data->client, data->name, value);
}

static CommandResult
handle_sticker_song(Client &client, unsigned argc, char *argv[])
{
	Error error;
	const Database *db = client.GetDatabase(error);
	if (db == nullptr)
		return print_error(client, error);

	/* get song song_id key */
	if (argc == 5 && strcmp(argv[1], "get") == 0) {
		const LightSong *song = db->GetSong(argv[3], error);
		if (song == nullptr)
			return print_error(client, error);

		const auto value = sticker_song_get_value(*song, argv[4]);
		db->ReturnSong(song);
		if (value.empty()) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such sticker");
			return CommandResult::ERROR;
		}

		sticker_print_value(client, argv[4], value.c_str());

		return CommandResult::OK;
	/* list song song_id */
	} else if (argc == 4 && strcmp(argv[1], "list") == 0) {
		const LightSong *song = db->GetSong(argv[3], error);
		if (song == nullptr)
			return print_error(client, error);

		sticker *sticker = sticker_song_get(*song);
		db->ReturnSong(song);
		if (sticker) {
			sticker_print(client, *sticker);
			sticker_free(sticker);
		}

		return CommandResult::OK;
	/* set song song_id id key */
	} else if (argc == 6 && strcmp(argv[1], "set") == 0) {
		const LightSong *song = db->GetSong(argv[3], error);
		if (song == nullptr)
			return print_error(client, error);

		bool ret = sticker_song_set_value(*song, argv[4], argv[5]);
		db->ReturnSong(song);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set sticker value");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	/* delete song song_id [key] */
	} else if ((argc == 4 || argc == 5) &&
		   strcmp(argv[1], "delete") == 0) {
		const LightSong *song = db->GetSong(argv[3], error);
		if (song == nullptr)
			return print_error(client, error);

		bool ret = argc == 4
			? sticker_song_delete(*song)
			: sticker_song_delete_value(*song, argv[4]);
		db->ReturnSong(song);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "no such sticker");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	/* find song dir key */
	} else if (argc == 5 && strcmp(argv[1], "find") == 0) {
		/* "sticker find song a/directory name" */

		const char *const base_uri = argv[3];

		bool success;
		struct sticker_song_find_data data = {
			client,
			argv[4],
		};

		success = sticker_song_find(*db, base_uri, data.name,
					    sticker_song_find_print_cb, &data);
		if (!success) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set search sticker database");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	} else {
		command_error(client, ACK_ERROR_ARG, "bad request");
		return CommandResult::ERROR;
	}
}

CommandResult
handle_sticker(Client &client, unsigned argc, char *argv[])
{
	assert(argc >= 4);

	if (!sticker_enabled()) {
		command_error(client, ACK_ERROR_UNKNOWN,
			      "sticker database is disabled");
		return CommandResult::ERROR;
	}

	if (strcmp(argv[2], "song") == 0)
		return handle_sticker_song(client, argc, argv);
	else {
		command_error(client, ACK_ERROR_ARG,
			      "unknown sticker domain");
		return CommandResult::ERROR;
	}
}
