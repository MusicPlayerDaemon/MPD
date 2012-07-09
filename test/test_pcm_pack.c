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
#include "pcm_pack.h"

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
test_pcm_pack_24(void)
{
	enum { N = 256 };
	int32_t src[N * 3];
	for (unsigned i = 0; i < N; ++i)
		src[i] = random24();

	uint8_t dest[N * 3];

	pcm_pack_24(dest, src, src + N);

	for (unsigned i = 0; i < N; ++i) {
		int32_t d;
		if (G_BYTE_ORDER == G_BIG_ENDIAN)
			d = (dest[i * 3] << 16) | (dest[i * 3 + 1] << 8)
				| dest[i * 3 + 2];
		else
			d = (dest[i * 3 + 2] << 16) | (dest[i * 3 + 1] << 8)
				| dest[i * 3];
		if (d & 0x800000)
			d |= 0xff000000;

		g_assert_cmpint(d, ==, src[i]);
	}
}

void
test_pcm_unpack_24(void)
{
	enum { N = 256 };
	uint8_t src[N * 3];
	for (unsigned i = 0; i < G_N_ELEMENTS(src); ++i)
		src[i] = g_random_int_range(0, 256);

	int32_t dest[N];

	pcm_unpack_24(dest, src, src + G_N_ELEMENTS(src));

	for (unsigned i = 0; i < N; ++i) {
		int32_t s;
		if (G_BYTE_ORDER == G_BIG_ENDIAN)
			s = (src[i * 3] << 16) | (src[i * 3 + 1] << 8)
				| src[i * 3 + 2];
		else
			s = (src[i * 3 + 2] << 16) | (src[i * 3 + 1] << 8)
				| src[i * 3];
		if (s & 0x800000)
			s |= 0xff000000;

		g_assert_cmpint(s, ==, dest[i]);
	}
}
