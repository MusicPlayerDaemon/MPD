// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * A parser for the AIFF file format.
 */

#ifndef MPD_AIFF_HXX
#define MPD_AIFF_HXX

#include "thread/Mutex.hxx"

#include <cstddef>

class InputStream;

/**
 * Seeks the AIFF file to the ID3 chunk.
 *
 * Throws std::runtime_error on error.
 *
 * @param is a locked #InputStream
 * @return the size of the ID3 chunk
 */
size_t
aiff_seek_id3(InputStream &is, std::unique_lock<Mutex> &lock);

#endif
