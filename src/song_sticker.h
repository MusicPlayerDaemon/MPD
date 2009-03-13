/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef SONG_STICKER_H
#define SONG_STICKER_H

#include <stdbool.h>
#include <glib.h>

struct song;

/**
 * Returns one value from a song's sticker record.  The caller must
 * free the return value with g_free().
 */
char *
sticker_song_get_value(const struct song *song, const char *name);

/**
 * Sets a sticker value in the specified song.  Overwrites existing
 * values.
 */
bool
sticker_song_set_value(const struct song *song,
		       const char *name, const char *value);

/**
 * Returns a hash table of key value pairs from a song's sticker record.
 * The caller must free the GHashTable with g_hash_table_destroy().
 */
GHashTable *
sticker_song_list_values(const struct song *song);

/**
 * Deletes a sticker from the database.  All values are deleted.
 */
bool
sticker_song_delete(const struct song *song);

#endif
