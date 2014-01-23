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

#ifndef MPD_PLAYLIST_PRINT_HXX
#define MPD_PLAYLIST_PRINT_HXX

#include <stdint.h>

struct playlist;
class SongFilter;
class Client;
class Error;

/**
 * Sends the whole playlist to the client, song URIs only.
 */
void
playlist_print_uris(Client &client, const playlist &playlist);

/**
 * Sends a range of the playlist to the client, including all known
 * information about the songs.  The "end" offset is decreased
 * automatically if it is too large; passing UINT_MAX is allowed.
 * This function however fails when the start offset is invalid.
 */
bool
playlist_print_info(Client &client, const playlist &playlist,
		    unsigned start, unsigned end);

/**
 * Sends the song with the specified id to the client.
 *
 * @return true on suite, false if there is no such song
 */
bool
playlist_print_id(Client &client, const playlist &playlist,
		  unsigned id);

/**
 * Sends the current song to the client.
 *
 * @return true on success, false if there is no current song
 */
bool
playlist_print_current(Client &client, const playlist &playlist);

/**
 * Find songs in the playlist.
 */
void
playlist_print_find(Client &client, const playlist &playlist,
		    const SongFilter &filter);

/**
 * Print detailed changes since the specified playlist version.
 */
void
playlist_print_changes_info(Client &client,
			    const playlist &playlist,
			    uint32_t version);

/**
 * Print changes since the specified playlist version, position only.
 */
void
playlist_print_changes_position(Client &client,
				const playlist &playlist,
				uint32_t version);

/**
 * Send the stored playlist to the client.
 *
 * @param client the client which requested the playlist
 * @param name_utf8 the name of the stored playlist in UTF-8 encoding
 * @param detail true if all details should be printed
 * @return true on success, false if the playlist does not exist
 */
bool
spl_print(Client &client, const char *name_utf8, bool detail,
	  Error &error);

#endif
