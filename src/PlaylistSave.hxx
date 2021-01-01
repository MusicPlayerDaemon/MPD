/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_PLAYLIST_SAVE_H
#define MPD_PLAYLIST_SAVE_H

struct Queue;
struct playlist;
class BufferedOutputStream;
class DetachedSong;

void
playlist_print_song(BufferedOutputStream &os, const DetachedSong &song);

void
playlist_print_uri(BufferedOutputStream &os, const char *uri);

/**
 * Saves a queue object into a stored playlist file.
 */
void
spl_save_queue(const char *name_utf8, const Queue &queue);

/**
 * Saves a playlist object into a stored playlist file.
 */
void
spl_save_playlist(const char *name_utf8, const playlist &playlist);

#endif
