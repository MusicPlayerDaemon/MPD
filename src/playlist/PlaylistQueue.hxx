// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*! \file
 * \brief Glue between playlist plugin and the play queue
 */

#ifndef MPD_PLAYLIST_QUEUE_HXX
#define MPD_PLAYLIST_QUEUE_HXX

class SongLoader;
class SongEnumerator;
struct playlist;
class PlayerControl;

/**
 * Loads the contents of a playlist and append it to the specified
 * play queue.
 *
 * @param uri the URI of the playlist, used to resolve relative song
 * URIs
 * @param start_index the index of the first song
 * @param end_index the index of the last song (excluding)
 */
void
playlist_load_into_queue(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader);

/**
 * Opens a playlist with a playlist plugin and append to the specified
 * play queue.
 */
void
playlist_open_into_queue(const LocatedUri &uri,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader);

#endif

