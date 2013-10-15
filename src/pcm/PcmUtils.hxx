/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_PCM_UTILS_H
#define MPD_PCM_UTILS_H

#include "Compiler.h"

#include <limits>

#include <stdint.h>

/**
 * Add a byte count to the specified pointer.  This is a utility
 * function to convert a source pointer and a byte count to an "end"
 * pointer for use in loops.
 */
template<typename T>
static inline const T *
pcm_end_pointer(const T *p, size_t size)
{
	return (const T *)((const uint8_t *)p + size);
}

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
template<typename T, typename U, unsigned bits>
gcc_const
static inline T
PcmClamp(U x)
{
	constexpr U MIN_VALUE = -(U(1) << (bits - 1));
	constexpr U MAX_VALUE = (U(1) << (bits - 1)) - 1;

	typedef std::numeric_limits<T> limits;
	static_assert(MIN_VALUE >= limits::min(), "out of range");
	static_assert(MAX_VALUE <= limits::max(), "out of range");

	if (gcc_unlikely(x < MIN_VALUE))
		return T(MIN_VALUE);

	if (gcc_unlikely(x > MAX_VALUE))
		return T(MAX_VALUE);

	return T(x);
}

#endif
