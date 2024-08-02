// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

class Response;
class SongLoader;
class SongFilter;
struct Partition;

/**
 * Send the playlist file to the client.
 *
 * Throws on error.
 *
 * @param uri the URI of the playlist file in UTF-8 encoding
 * @param detail true if all details should be printed
 */
void
playlist_file_print(Response &r, Partition &partition,
		    const SongLoader &loader,
		    const LocatedUri &uri,
		    unsigned start_index, unsigned end_index,
		    bool detail,
		    SongFilter *filter);

