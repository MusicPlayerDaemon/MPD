// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"

#include <memory>

class SongEnumerator;
class Path;

/**
 * Opens a playlist from a local file.
 *
 * @param path the path of the playlist file
 * @return a playlist, or nullptr on error
 */
std::unique_ptr<SongEnumerator>
playlist_open_path(Path path, Mutex &mutex);

/**
 * Opens a playlist from a remote file.
 *
 * @param uri the absolute URI of the playlist file
 * @return a playlist, or nullptr on error
 */
[[gnu::nonnull]]
std::unique_ptr<SongEnumerator>
playlist_open_remote(const char *uri, Mutex &mutex);
