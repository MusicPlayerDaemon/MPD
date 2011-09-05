/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "dbUtils.h"
#include "locate.h"
#include "database.h"
#include "playlist.h"
#include "stored_playlist.h"

#include <glib.h>

static int
directoryAddSongToPlaylist(struct song *song, void *data)
{
	struct player_control *pc = data;

	return playlist_append_song(&g_playlist, pc, song, NULL);
}

struct add_data {
	const char *path;
};

static int
directoryAddSongToStoredPlaylist(struct song *song, void *_data)
{
	struct add_data *data = _data;

	if (spl_append_song(data->path, song) != 0)
		return -1;
	return 0;
}

int
addAllIn(struct player_control *pc, const char *name)
{
	return db_walk(name, directoryAddSongToPlaylist, NULL, pc);
}

int addAllInToStoredPlaylist(const char *name, const char *utf8file)
{
	struct add_data data = {
		.path = utf8file,
	};

	return db_walk(name, directoryAddSongToStoredPlaylist, NULL, &data);
}

struct find_add_data {
	struct player_control *pc;
	const struct locate_item_list *criteria;
};

static int
findAddInDirectory(struct song *song, void *_data)
{
	struct find_add_data *data = _data;

	if (locate_song_match(song, data->criteria))
		return playlist_append_song(&g_playlist,
					    data->pc,
					    song, NULL);

	return 0;
}

int
findAddIn(struct player_control *pc, const char *name,
	  const struct locate_item_list *criteria)
{
	struct find_add_data data;
	data.pc = pc;
	data.criteria = criteria;

	return db_walk(name, findAddInDirectory, NULL, &data);
}
