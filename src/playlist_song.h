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

#ifndef MPD_PLAYLIST_SONG_H
#define MPD_PLAYLIST_SONG_H

#include <stdbool.h>

/**
 * Verifies the song, returns NULL if it is unsafe.  Translate the
 * song to a new song object within the database, if it is a local
 * file.  The old song object is freed.
 *
 * @param secure if true, then local files are only allowed if they
 * are relative to base_uri
 */
struct song *
playlist_check_translate_song(struct song *song, const char *base_uri,
			      bool secure);

#endif
