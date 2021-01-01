/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_PCM_MIX_HXX
#define MPD_PCM_MIX_HXX

#include "SampleFormat.hxx"
#include "util/Compiler.h"

#include <cstddef>

class PcmDither;

/*
 * Linearly mixes two PCM buffers.  Both must have the same length and
 * the same audio format.  The formula is:
 *
 *   s1 := s1 * portion1 + s2 * (1 - portion1)
 *
 * @param buffer1 the first PCM buffer, and the destination buffer
 * @param buffer2 the second PCM buffer
 * @param size the size of both buffers in bytes
 * @param format the sample format of both buffers
 * @param portion1 a number between 0.0 and 1.0 specifying the portion
 * of the first buffer in the mix; portion2 = (1.0 - portion1).
 * Negative values are used by the MixRamp code to specify that simple
 * addition is required.
 *
 * @return true on success, false if the format is not supported
 */
[[nodiscard]]
bool
pcm_mix(PcmDither &dither, void *buffer1, const void *buffer2, size_t size,
	SampleFormat format, float portion1) noexcept;

#endif
