/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "pcm_dither.h"
#include "pcm_prng.h"

static int16_t
pcm_dither_sample_24_to_16(int32_t sample, struct pcm_dither *dither)
{
	int32_t output, rnd;

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

	sample += dither->error[0] - dither->error[1] + dither->error[2];

	dither->error[2] = dither->error[1];
	dither->error[1] = dither->error[0] / 2;

	/* round */
	output = sample + round;

	rnd = pcm_prng(dither->random);
	output += (rnd & mask) - (dither->random & mask);

	dither->random = rnd;

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

	dither->error[0] = sample - output;

	return (int16_t)(output >> scale_bits);
}

void
pcm_dither_24_to_16(struct pcm_dither *dither,
		    int16_t *dest, const int32_t *src,
		    unsigned num_samples)
{
	while (num_samples-- > 0)
		*dest++ = pcm_dither_sample_24_to_16(*src++, dither);
}

static int16_t
pcm_dither_sample_32_to_16(int32_t sample, struct pcm_dither *dither)
{
	return pcm_dither_sample_24_to_16(sample >> 8, dither);
}

void
pcm_dither_32_to_16(struct pcm_dither *dither,
		    int16_t *dest, const int32_t *src,
		    unsigned num_samples)
{
	while (num_samples-- > 0)
		*dest++ = pcm_dither_sample_32_to_16(*src++, dither);
}
