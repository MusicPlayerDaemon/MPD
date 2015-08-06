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

#include "config.h"
#include "StickerCommands.hxx"
#include "Request.hxx"
#include "SongPrint.hxx"
#include "db/Interface.hxx"
#include "db/DatabaseGlue.hxx"
#include "sticker/SongSticker.hxx"
#include "sticker/StickerPrint.hxx"
#include "sticker/StickerDatabase.hxx"
#include "CommandError.hxx"
#include "client/Client.hxx"
#include "client/Response.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "util/Error.hxx"
#include "util/ConstBuffer.hxx"

#include <string.h>

struct sticker_song_find_data {
	Response &r;
	Partition &partition;
	const char *name;
};

static void
sticker_song_find_print_cb(const LightSong &song, const char *value,
			   void *user_data)
{
	struct sticker_song_find_data *data =
		(struct sticker_song_find_data *)user_data;

	song_print_uri(data->r, data->partition, song);
	sticker_print_value(data->r, data->name, value);
}

static CommandResult
handle_sticker_song(Response &r, Partition &partition, Request args)
{
	Error error;
	const Database *db = partition.GetDatabase(error);
	if (db == nullptr)
		return print_error(r, error);

	const char *const cmd = args.front();

	/* get song song_id key */
	if (args.size == 4 && strcmp(cmd, "get") == 0) {
		const LightSong *song = db->GetSong(args[2], error);
		if (song == nullptr)
			return print_error(r, error);

		const auto value = sticker_song_get_value(*song, args[3],
							  error);
		db->ReturnSong(song);
		if (value.empty()) {
			if (error.IsDefined())
				return print_error(r, error);

			r.Error(ACK_ERROR_NO_EXIST, "no such sticker");
			return CommandResult::ERROR;
		}

		sticker_print_value(r, args[3], value.c_str());

		return CommandResult::OK;
	/* list song song_id */
	} else if (args.size == 3 && strcmp(cmd, "list") == 0) {
		const LightSong *song = db->GetSong(args[2], error);
		if (song == nullptr)
			return print_error(r, error);

		Sticker *sticker = sticker_song_get(*song, error);
		db->ReturnSong(song);
		if (sticker) {
			sticker_print(r, *sticker);
			sticker_free(sticker);
		} else if (error.IsDefined())
			return print_error(r, error);

		return CommandResult::OK;
	/* set song song_id id key */
	} else if (args.size == 5 && strcmp(cmd, "set") == 0) {
		const LightSong *song = db->GetSong(args[2], error);
		if (song == nullptr)
			return print_error(r, error);

		bool ret = sticker_song_set_value(*song, args[3], args[4],
						  error);
		db->ReturnSong(song);
		if (!ret) {
			if (error.IsDefined())
				return print_error(r, error);

			r.Error(ACK_ERROR_SYSTEM,
				"failed to set sticker value");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	/* delete song song_id [key] */
	} else if ((args.size == 3 || args.size == 4) &&
		   strcmp(cmd, "delete") == 0) {
		const LightSong *song = db->GetSong(args[2], error);
		if (song == nullptr)
			return print_error(r, error);

		bool ret = args.size == 3
			? sticker_song_delete(*song, error)
			: sticker_song_delete_value(*song, args[3], error);
		db->ReturnSong(song);
		if (!ret) {
			if (error.IsDefined())
				return print_error(r, error);

			r.Error(ACK_ERROR_SYSTEM, "no such sticker");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	/* find song dir key */
	} else if ((args.size == 4 || args.size == 6) &&
		   strcmp(cmd, "find") == 0) {
		/* "sticker find song a/directory name" */

		const char *const base_uri = args[2];

		StickerOperator op = StickerOperator::EXISTS;
		const char *value = nullptr;

		if (args.size == 6) {
			/* match the value */

			const char *op_s = args[4];
			value = args[5];

			if (strcmp(op_s, "=") == 0)
				op = StickerOperator::EQUALS;
			else if (strcmp(op_s, "<") == 0)
				op = StickerOperator::LESS_THAN;
			else if (strcmp(op_s, ">") == 0)
				op = StickerOperator::GREATER_THAN;
			else {
				r.Error(ACK_ERROR_ARG, "bad operator");
				return CommandResult::ERROR;
			}
		}

		struct sticker_song_find_data data = {
			r,
			partition,
			args[3],
		};

		if (!sticker_song_find(*db, base_uri, data.name,
				       op, value,
				       sticker_song_find_print_cb, &data,
				       error)) {
			if (error.IsDefined())
				return print_error(r, error);

			r.Error(ACK_ERROR_SYSTEM,
				"failed to set search sticker database");
			return CommandResult::ERROR;
		}

		return CommandResult::OK;
	} else {
		r.Error(ACK_ERROR_ARG, "bad request");
		return CommandResult::ERROR;
	}
}

CommandResult
handle_sticker(Client &client, Request args)
{
	Response r(client);

	assert(args.size >= 3);

	if (!sticker_enabled()) {
		r.Error(ACK_ERROR_UNKNOWN, "sticker database is disabled");
		return CommandResult::ERROR;
	}

	if (strcmp(args[1], "song") == 0)
		return handle_sticker_song(r, client.partition, args);
	else {
		r.Error(ACK_ERROR_ARG, "unknown sticker domain");
		return CommandResult::ERROR;
	}
}
