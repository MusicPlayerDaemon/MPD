/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
