// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "thread/Mutex.hxx"
#include "config.h"

#include <memory>

class SongEnumerator;
class Storage;

/**
 * Opens a playlist from the specified URI, which can be either an
 * absolute remote URI (with a scheme) or a relative path to the
 * music or playlist directory.
 *
 * Throws on error.
 *
 * @return a playlist, or nullptr if the file is not supported
 */
std::unique_ptr<SongEnumerator>
playlist_open_any(const LocatedUri &located_uri,
#ifdef ENABLE_DATABASE
		  Storage *storage,
#endif
		  Mutex &mutex);
