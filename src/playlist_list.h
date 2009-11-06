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

#ifndef MPD_PLAYLIST_LIST_H
#define MPD_PLAYLIST_LIST_H

struct playlist_provider;
struct input_stream;

/**
 * Initializes all playlist plugins.
 */
void
playlist_list_global_init(void);

/**
 * Deinitializes all playlist plugins.
 */
void
playlist_list_global_finish(void);

/**
 * Opens a playlist by its URI.
 */
struct playlist_provider *
playlist_list_open_uri(const char *uri);

/**
 * Opens a playlist from an input stream.
 *
 * @param is an #input_stream object which is open and ready
 * @param uri optional URI which was used to open the stream; may be
 * used to select the appropriate playlist plugin
 */
struct playlist_provider *
playlist_list_open_stream(struct input_stream *is, const char *uri);

/**
 * Opens a playlist from a local file.
 *
 * @param is an uninitialized #input_stream object (must be closed
 * with input_stream_close() if this function succeeds)
 * @param path_fs the path of the playlist file
 * @return a playlist, or NULL on error
 */
struct playlist_provider *
playlist_list_open_path(struct input_stream *is, const char *path_fs);

#endif
