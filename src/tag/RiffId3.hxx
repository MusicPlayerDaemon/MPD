// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/** \file
 *
 * A parser for the RIFF file format (e.g. WAV).
 */

#ifndef MPD_RIFF_ID3_HXX
#define MPD_RIFF_ID3_HXX

#include "thread/Mutex.hxx"

#include <cstddef>

class InputStream;

/**
 * Seeks the RIFF file to the ID3 chunk.
 *
 * Throws std::runtime_error on error.
 *
 * @param is a locked #InputStream
 * @return the size of the ID3 chunk
 */
size_t
riff_seek_id3(InputStream &is, std::unique_lock<Mutex> &lock);

#endif
