// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_STREAM_HXX
#define MPD_PLAYLIST_STREAM_HXX

#include "thread/Mutex.hxx"
#include "util/Compiler.h"

#include <memory>

class SongEnumerator;
class Path;

/**
 * Opens a playlist from a local file.
 *
 * @param path the path of the playlist file
 * @return a playlist, or nullptr on error
 */
gcc_nonnull_all
std::unique_ptr<SongEnumerator>
playlist_open_path(Path path, Mutex &mutex);

gcc_nonnull_all
std::unique_ptr<SongEnumerator>
playlist_open_remote(const char *uri, Mutex &mutex);

#endif
