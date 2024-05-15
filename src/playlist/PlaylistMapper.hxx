// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"
#include "config.h"

#include <memory>

class SongEnumerator;
class Storage;

/**
 * Opens a playlist from an URI relative to the playlist or music
 * directory.
 *
 * Throws on error.
 *
 * @return a playlist, or nullptr if the file is not supported
 */
std::unique_ptr<SongEnumerator>
playlist_mapper_open(const char *uri,
#ifdef ENABLE_DATABASE
		     Storage *storage,
#endif
		     Mutex &mutex);
