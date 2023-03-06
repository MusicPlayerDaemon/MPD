// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_DATABASE_HXX
#define MPD_PLAYLIST_DATABASE_HXX

#define PLAYLIST_META_BEGIN "playlist_begin: "

class PlaylistVector;
class BufferedOutputStream;
class LineReader;

void
playlist_vector_save(BufferedOutputStream &os, const PlaylistVector &pv);

/**
 * Throws #std::runtime_error on error.
 */
void
playlist_metadata_load(LineReader &file, PlaylistVector &pv,
		       const char *name);

#endif
