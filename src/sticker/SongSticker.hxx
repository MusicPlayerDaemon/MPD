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

#ifndef MPD_SONG_STICKER_HXX
#define MPD_SONG_STICKER_HXX

#include "Compiler.h"

#include <string>

struct LightSong;
struct sticker;
class Database;

/**
 * Returns one value from a song's sticker record.  The caller must
 * free the return value with g_free().
 */
gcc_pure
std::string
sticker_song_get_value(const LightSong &song, const char *name);

/**
 * Sets a sticker value in the specified song.  Overwrites existing
 * values.
 */
bool
sticker_song_set_value(const LightSong &song,
		       const char *name, const char *value);

/**
 * Deletes a sticker from the database.  All values are deleted.
 */
bool
sticker_song_delete(const LightSong &song);

/**
 * Deletes a sticker value.  Does nothing if the sticker did not
 * exist.
 */
bool
sticker_song_delete_value(const LightSong &song, const char *name);

/**
 * Loads the sticker for the specified song.
 *
 * @param song the song object
 * @return a sticker object, or NULL on error or if there is no sticker
 */
sticker *
sticker_song_get(const LightSong &song);

/**
 * Finds stickers with the specified name below the specified
 * directory.
 *
 * Caller must lock the #db_mutex.
 *
 * @param base_uri the base directory to search in
 * @param name the name of the sticker
 * @return true on success (even if no sticker was found), false on
 * failure
 */
bool
sticker_song_find(const Database &db, const char *base_uri, const char *name,
		  void (*func)(const LightSong &song, const char *value,
			       void *user_data),
		  void *user_data);

#endif
