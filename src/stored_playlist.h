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

#ifndef MPD_STORED_PLAYLIST_H
#define MPD_STORED_PLAYLIST_H

#include "playlist.h"

#include <glib.h>
#include <stdbool.h>
#include <time.h>

struct song;

struct stored_playlist_info {
	char *name;

	time_t mtime;
};

extern bool playlist_saveAbsolutePaths;

/**
 * Perform some global initialization, e.g. load configuration values.
 */
void
spl_global_init(void);

/**
 * Determines whether the specified string is a valid name for a
 * stored playlist.
 */
bool
spl_valid_name(const char *name_utf8);

/**
 * Returns a list of stored_playlist_info struct pointers.  Returns
 * NULL if an error occured.
 */
GPtrArray *
spl_list(void);

void
spl_list_free(GPtrArray *list);

GPtrArray *
spl_load(const char *utf8path);

void
spl_free(GPtrArray *list);

enum playlist_result
spl_move_index(const char *utf8path, unsigned src, unsigned dest);

enum playlist_result
spl_clear(const char *utf8path);

enum playlist_result
spl_delete(const char *name_utf8);

enum playlist_result
spl_remove_index(const char *utf8path, unsigned pos);

enum playlist_result
spl_append_song(const char *utf8path, struct song *song);

enum playlist_result
spl_append_uri(const char *file, const char *utf8file);

enum playlist_result
spl_rename(const char *utf8from, const char *utf8to);

#endif
