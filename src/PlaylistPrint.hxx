// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
