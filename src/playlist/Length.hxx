// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST__LENGTH_HXX
#define MPD_PLAYLIST__LENGTH_HXX

#include "client/Response.hxx"

class SongLoader;
struct Partition;

/**
 * Count the number of songs and their total playtime (seconds) in the
 * playlist.
 *
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @return true on success, false if the playlist does not exist
 */
bool
playlist_file_length(Response &r, Partition &partition,
		    const SongLoader &loader,
		    const LocatedUri &uri);

#endif
