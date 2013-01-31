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

inline int16_t
PcmDither::Dither24To16(int_fast32_t sample)
{
	enum {
		from_bits = 24,
		to_bits = 16,
		scale_bits = from_bits - to_bits,
		round = 1 << (scale_bits - 1),
		mask = (1 << scale_bits) - 1,
		ONE = 1 << (from_bits - 1),
		MIN = -ONE,
		MAX = ONE - 1
	};

	sample += error[0] - error[1] + error[2];

	error[2] = error[1];
	error[1] = error[0] / 2;

	/* round */
	int_fast32_t output = sample + round;

	int_fast32_t rnd = pcm_prng(random);
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

	return (int16_t)(output >> scale_bits);
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
