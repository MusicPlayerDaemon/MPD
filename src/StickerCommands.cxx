/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "DatabaseLock.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseGlue.hxx"
#include "DatabaseSimple.hxx"
#include "SongSticker.hxx"
#include "StickerPrint.hxx"
#include "StickerDatabase.hxx"
#include "CommandError.hxx"

extern "C" {
#include "protocol/result.h"
}

#include <string.h>

struct sticker_song_find_data {
	struct client *client;
	const char *name;
};

static void
sticker_song_find_print_cb(struct song *song, const char *value,
			   gpointer user_data)
{
	struct sticker_song_find_data *data =
		(struct sticker_song_find_data *)user_data;

	song_print_uri(data->client, song);
	sticker_print_value(data->client, data->name, value);
}

static enum command_return
handle_sticker_song(struct client *client, int argc, char *argv[])
{
	GError *error = nullptr;
	const Database *db = GetDatabase(&error);
	if (db == nullptr)
		return print_error(client, error);

	/* get song song_id key */
	if (argc == 5 && strcmp(argv[1], "get") == 0) {
		song *song = db->GetSong(argv[3], &error);
		if (song == nullptr)
			return print_error(client, error);

		char *value = sticker_song_get_value(song, argv[4]);
		db->ReturnSong(song);
		if (value == NULL) {
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such sticker");
			return COMMAND_RETURN_ERROR;
		}

		sticker_print_value(client, argv[4], value);
		g_free(value);

		return COMMAND_RETURN_OK;
	/* list song song_id */
	} else if (argc == 4 && strcmp(argv[1], "list") == 0) {
		song *song = db->GetSong(argv[3], &error);
		if (song == nullptr)
			return print_error(client, error);

		sticker *sticker = sticker_song_get(song);
		db->ReturnSong(song);
		if (sticker) {
			sticker_print(client, sticker);
			sticker_free(sticker);
		}

		return COMMAND_RETURN_OK;
	/* set song song_id id key */
	} else if (argc == 6 && strcmp(argv[1], "set") == 0) {
		song *song = db->GetSong(argv[3], &error);
		if (song == nullptr)
			return print_error(client, error);

		bool ret = sticker_song_set_value(song, argv[4], argv[5]);
		db->ReturnSong(song);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set sticker value");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	/* delete song song_id [key] */
	} else if ((argc == 4 || argc == 5) &&
		   strcmp(argv[1], "delete") == 0) {
		song *song = db->GetSong(argv[3], &error);
		if (song == nullptr)
			return print_error(client, error);

		bool ret = argc == 4
			? sticker_song_delete(song)
			: sticker_song_delete_value(song, argv[4]);
		db->ReturnSong(song);
		if (!ret) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "no such sticker");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	/* find song dir key */
	} else if (argc == 5 && strcmp(argv[1], "find") == 0) {
		/* "sticker find song a/directory name" */
		struct directory *directory;
		bool success;
		struct sticker_song_find_data data = {
			client,
			argv[4],
		};

		db_lock();
		directory = db_get_directory(argv[3]);
		if (directory == NULL) {
			db_unlock();
			command_error(client, ACK_ERROR_NO_EXIST,
				      "no such directory");
			return COMMAND_RETURN_ERROR;
		}

		success = sticker_song_find(directory, data.name,
					    sticker_song_find_print_cb, &data);
		db_unlock();
		if (!success) {
			command_error(client, ACK_ERROR_SYSTEM,
				      "failed to set search sticker database");
			return COMMAND_RETURN_ERROR;
		}

		return COMMAND_RETURN_OK;
	} else {
		command_error(client, ACK_ERROR_ARG, "bad request");
		return COMMAND_RETURN_ERROR;
	}
}

enum command_return
handle_sticker(struct client *client, int argc, char *argv[])
{
	assert(argc >= 4);

	if (!sticker_enabled()) {
		command_error(client, ACK_ERROR_UNKNOWN,
			      "sticker database is disabled");
		return COMMAND_RETURN_ERROR;
	}

	if (strcmp(argv[2], "song") == 0)
		return handle_sticker_song(client, argc, argv);
	else {
		command_error(client, ACK_ERROR_ARG,
			      "unknown sticker domain");
		return COMMAND_RETURN_ERROR;
	}
}
