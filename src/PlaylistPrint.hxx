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

#ifndef MPD_PLAYLIST_PRINT_HXX
#define MPD_PLAYLIST_PRINT_HXX

#include <cstdint>

struct playlist;
struct RangeArg;
struct QueueSelection;
class Response;

/**
 * Sends the whole playlist to the client, song URIs only.
 */
void
playlist_print_uris(Response &r, const playlist &playlist);

/**
 * Sends a range of the playlist to the client, including all known
 * information about the songs.  The "end" offset is decreased
 * automatically if it is too large; passing UINT_MAX is allowed.
 * This function however fails when the start offset is invalid.
 *
 * Throws #PlaylistError if the range is invalid.
 */
void
playlist_print_info(Response &r, const playlist &playlist, RangeArg range);

/**
 * Sends the song with the specified id to the client.
 *
 * Throws #PlaylistError if the range is invalid.
 */
void
playlist_print_id(Response &r, const playlist &playlist, unsigned id);

/**
 * Sends the current song to the client.
 *
 * @return true on success, false if there is no current song
 */
bool
playlist_print_current(Response &r, const playlist &playlist);

/**
 * Find songs in the playlist.
 */
void
playlist_print_find(Response &r, const playlist &playlist,
		    const QueueSelection &selection);

/**
 * Print detailed changes since the specified playlist version.
 */
void
playlist_print_changes_info(Response &r, const playlist &playlist,
			    uint32_t version,
			    RangeArg range);

/**
 * Print changes since the specified playlist version, position only.
 */
void
playlist_print_changes_position(Response &r,
				const playlist &playlist,
				uint32_t version,
				RangeArg range);

#endif
