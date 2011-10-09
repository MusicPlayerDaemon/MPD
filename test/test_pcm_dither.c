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

#include "test_pcm_all.h"
#include "pcm_dither.h"

#include <glib.h>

/**
 * Generate a random 24 bit PCM sample.
 */
static int32_t
random24(void)
{
	int32_t x = g_random_int() & 0xffffff;
	if (x & 0x800000)
		x |= 0xff000000;
	return x;
}

void
test_pcm_dither_24(void)
{
	struct pcm_dither dither;

	pcm_dither_24_init(&dither);

	enum { N = 256 };
	int32_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = random24();

	int16_t dest[N];

	pcm_dither_24_to_16(&dither, dest, src, src + N);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] >> 8) - 8);
		g_assert_cmpint(dest[i], <, (src[i] >> 8) + 8);
	}
}

void
test_pcm_dither_32(void)
{
	struct pcm_dither dither;

	pcm_dither_24_init(&dither);

	enum { N = 256 };
	int32_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = g_random_int();

	int16_t dest[N];

	pcm_dither_32_to_16(&dither, dest, src, src + N);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] >> 16) - 8);
		g_assert_cmpint(dest[i], <, (src[i] >> 16) + 8);
	}
}
