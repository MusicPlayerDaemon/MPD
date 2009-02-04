/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef MPD_PLAYLIST_SAVE_H
#define MPD_PLAYLIST_SAVE_H

#include "playlist.h"

#include <stdio.h>

struct song;

void
playlist_print_song(FILE *fp, const struct song *song);

void
playlist_print_uri(FILE *fp, const char *uri);

/**
 * Saves a queue object into a stored playlist file.
 */
enum playlist_result
spl_save_queue(const char *name_utf8, const struct queue *queue);

/**
 * Saves a playlist object into a stored playlist file.
 */
enum playlist_result
spl_save_playlist(const char *name_utf8, const struct playlist *playlist);

/**
 * Loads a stored playlist file, and append all songs to the global
 * playlist.
 */
enum playlist_result
playlist_load_spl(struct playlist *playlist, const char *name_utf8);

#endif
