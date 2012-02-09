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

/*! \file
 * \brief Glue between playlist plugin and the play queue
 */

#ifndef MPD_PLAYLIST_QUEUE_H
#define MPD_PLAYLIST_QUEUE_H

#include "playlist_error.h"

#include <stdbool.h>

struct playlist_provider;
struct playlist;
struct player_control;

/**
 * Loads the contents of a playlist and append it to the specified
 * play queue.
 *
 * @param uri the URI of the playlist, used to resolve relative song
 * URIs
 * @param start_index the index of the first song
 * @param end_index the index of the last song (excluding)
 */
enum playlist_result
playlist_load_into_queue(const char *uri, struct playlist_provider *source,
			 unsigned start_index, unsigned end_index,
			 struct playlist *dest, struct player_control *pc,
			 bool secure);

/**
 * Opens a playlist with a playlist plugin and append to the specified
 * play queue.
 */
enum playlist_result
playlist_open_into_queue(const char *uri,
			 unsigned start_index, unsigned end_index,
			 struct playlist *dest, struct player_control *pc,
			 bool secure);

#endif

