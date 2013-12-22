/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "PcmDither.hxx"
#include "PcmPrng.hxx"

template<typename T, T MIN, T MAX, unsigned scale_bits>
T
PcmDither::Dither(T sample)
{
	constexpr T round = 1 << (scale_bits - 1);
	constexpr T mask = (1 << scale_bits) - 1;

	sample += error[0] - error[1] + error[2];

	error[2] = error[1];
	error[1] = error[0] / 2;

	/* round */
	T output = sample + round;

	const T rnd = pcm_prng(random);
	output += (rnd & mask) - (random & mask);

	random = rnd;

	/* clip */
	if (output > MAX) {
		output = MAX;

		if (sample > MAX)
			sample = MAX;
	} else if (output < MIN) {
		output = MIN;

		if (sample < MIN)
			sample = MIN;
	}

	output &= ~mask;

	error[0] = sample - output;

	return output;
}

inline int16_t
PcmDither::Dither24To16(int_fast32_t sample)
{
	typedef decltype(sample) T;
	constexpr unsigned from_bits = 24;
	constexpr unsigned to_bits = 16;
	constexpr unsigned scale_bits = from_bits - to_bits;
	constexpr int_fast32_t ONE = 1 << (from_bits - 1);
	constexpr int_fast32_t MIN = -ONE;
	constexpr int_fast32_t MAX = ONE - 1;

	return Dither<T, MIN, MAX, scale_bits>(sample) >> scale_bits;
}

void
PcmDither::Dither24To16(int16_t *dest, const int32_t *src,
			const int32_t *src_end)
{
	while (src < src_end)
		*dest++ = Dither24To16(*src++);
}

inline int16_t
PcmDither::Dither32To16(int_fast32_t sample)
{
	return Dither24To16(sample >> 8);
}

void
PcmDither::Dither32To16(int16_t *dest, const int32_t *src,
			const int32_t *src_end)
{
	while (src < src_end)
		*dest++ = Dither32To16(*src++);
}
