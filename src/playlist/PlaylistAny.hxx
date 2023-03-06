// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_ANY_HXX
#define MPD_PLAYLIST_ANY_HXX

#include "thread/Mutex.hxx"
#include "config.h"

#include <memory>

class SongEnumerator;
class Storage;

/**
 * Opens a playlist from the specified URI, which can be either an
 * absolute remote URI (with a scheme) or a relative path to the
 * music or playlist directory.
 */
std::unique_ptr<SongEnumerator>
playlist_open_any(const LocatedUri &located_uri,
#ifdef ENABLE_DATABASE
		  const Storage *storage,
#endif
		  Mutex &mutex);

#endif
