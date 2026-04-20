// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"

#include <memory>
#include <string_view>

class SongEnumerator;
class Path;

/**
 * Opens a playlist from a local file.
 *
 * Throws on error.
 *
 * @param path the path of the playlist file
 * @return a playlist, or nullptr if the file is not supported
 */
std::unique_ptr<SongEnumerator>
playlist_open_path(Path path, Mutex &mutex);

/**
 * Opens a playlist from a remote file.
 *
 * Throws on error.
 *
 * @param uri the absolute URI of the playlist file
 * @return a playlist, or nullptr if the file is not supported
 */
std::unique_ptr<SongEnumerator>
playlist_open_remote(std::string_view uri, Mutex &mutex);
