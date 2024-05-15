// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "client/Response.hxx"

class SongLoader;
struct Partition;

/**
 * Count the number of songs and their total playtime (seconds) in the
 * playlist.
 *
 * Throws on error.
 *
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @return true on success, false if the playlist does not exist
 */
void
playlist_file_length(Response &r, Partition &partition,
		     const SongLoader &loader,
		     const LocatedUri &uri);
