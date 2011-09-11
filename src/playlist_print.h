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

#ifndef PLAYLIST_PRINT_H
#define PLAYLIST_PRINT_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

struct client;
struct playlist;
struct locate_item_list;

/**
 * Sends the whole playlist to the client, song URIs only.
 */
void
playlist_print_uris(struct client *client, const struct playlist *playlist);

/**
 * Sends a range of the playlist to the client, including all known
 * information about the songs.  The "end" offset is decreased
 * automatically if it is too large; passing UINT_MAX is allowed.
 * This function however fails when the start offset is invalid.
 */
bool
playlist_print_info(struct client *client, const struct playlist *playlist,
		    unsigned start, unsigned end);

/**
 * Sends the song with the specified id to the client.
 *
 * @return true on suite, false if there is no such song
 */
bool
playlist_print_id(struct client *client, const struct playlist *playlist,
		  unsigned id);

/**
 * Sends the current song to the client.
 *
 * @return true on success, false if there is no current song
 */
bool
playlist_print_current(struct client *client, const struct playlist *playlist);

/**
 * Find songs in the playlist.
 */
void
playlist_print_find(struct client *client, const struct playlist *playlist,
		    const struct locate_item_list *list);

/**
 * Search for songs in the playlist.
 */
void
playlist_print_search(struct client *client, const struct playlist *playlist,
		      const struct locate_item_list *list);

/**
 * Print detailed changes since the specified playlist version.
 */
void
playlist_print_changes_info(struct client *client,
			    const struct playlist *playlist,
			    uint32_t version);

/**
 * Print changes since the specified playlist version, position only.
 */
void
playlist_print_changes_position(struct client *client,
				const struct playlist *playlist,
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
spl_print(struct client *client, const char *name_utf8, bool detail,
	  GError **error_r);

/**
 * Send the playlist file to the client.
 *
 * @param client the client which requested the playlist
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @param detail true if all details should be printed
 * @return true on success, false if the playlist does not exist
 */
bool
playlist_file_print(struct client *client, const char *uri, bool detail);

#endif
