// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_MAPPER_HXX
#define MPD_PLAYLIST_MAPPER_HXX

#include "thread/Mutex.hxx"
#include "config.h"

#include <memory>

class SongEnumerator;
class Storage;

/**
 * Opens a playlist from an URI relative to the playlist or music
 * directory.
 */
std::unique_ptr<SongEnumerator>
playlist_mapper_open(const char *uri,
#ifdef ENABLE_DATABASE
		     const Storage *storage,
#endif
		     Mutex &mutex);

#endif
