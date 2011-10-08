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

#include <glib.h>

#include <stdint.h>

/**
 * Add a byte count to the specified pointer.  This is a utility
 * function to convert a source pointer and a byte count to an "end"
 * pointer for use in loops.
 */
static inline const void *
pcm_end_pointer(const void *p, size_t size)
{
	return (const char *)p + size;
}

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
static inline int32_t
pcm_range(int32_t sample, unsigned bits)
{
	if (G_UNLIKELY(sample < (-1 << (bits - 1))))
		return -1 << (bits - 1);
	if (G_UNLIKELY(sample >= (1 << (bits - 1))))
		return (1 << (bits - 1)) - 1;
	return sample;
}

/**
 * Check if the value is within the range of the provided bit size,
 * and caps it if necessary.
 */
static inline int64_t
pcm_range_64(int64_t sample, unsigned bits)
{
	if (G_UNLIKELY(sample < ((int64_t)-1 << (bits - 1))))
		return (int64_t)-1 << (bits - 1);
	if (G_UNLIKELY(sample >= ((int64_t)1 << (bits - 1))))
		return ((int64_t)1 << (bits - 1)) - 1;
	return sample;
}

G_GNUC_CONST
static inline int16_t
pcm_clamp_16(int x)
{
	static const int32_t MIN_VALUE = G_MININT16;
	static const int32_t MAX_VALUE = G_MAXINT16;

	if (G_UNLIKELY(x < MIN_VALUE))
		return MIN_VALUE;
	if (G_UNLIKELY(x > MAX_VALUE))
		return MAX_VALUE;
	return x;
}

G_GNUC_CONST
static inline int32_t
pcm_clamp_24(int x)
{
	static const int32_t MIN_VALUE = -(1 << 23);
	static const int32_t MAX_VALUE = (1 << 23) - 1;

	if (G_UNLIKELY(x < MIN_VALUE))
		return MIN_VALUE;
	if (G_UNLIKELY(x > MAX_VALUE))
		return MAX_VALUE;
	return x;
}

#endif
