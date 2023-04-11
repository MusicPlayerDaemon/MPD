// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_SAVE_HXX
#define MPD_SONG_SAVE_HXX

#include <memory>

#define SONG_BEGIN "song_begin: "

struct Song;
struct AudioFormat;
class DetachedSong;
class BufferedOutputStream;
class LineReader;

void
song_save(BufferedOutputStream &os, const Song &song);

void
song_save(BufferedOutputStream &os, const DetachedSong &song);

/**
 * Loads a song from the input file.  Reading stops after the
 * "song_end" line.
 *
 * Throws on error.
 */
DetachedSong
song_load(LineReader &file, const char *uri,
	  std::string *target_r=nullptr, bool *in_playlist_r=nullptr);

#endif
