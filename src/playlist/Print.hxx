// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST__PRINT_HXX
#define MPD_PLAYLIST__PRINT_HXX

class Response;
class SongLoader;
struct Partition;

/**
 * Send the playlist file to the client.
 *
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @param detail true if all details should be printed
 * @return true on success, false if the playlist does not exist
 */
bool
playlist_file_print(Response &r, Partition &partition,
		    const SongLoader &loader,
		    const LocatedUri &uri, bool detail);

#endif
