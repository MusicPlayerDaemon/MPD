/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "pcm_volume.h"

#include <glib.h>

#include <string.h>

void
test_pcm_volume_8(void)
{
	enum { N = 256 };
	static const int8_t zero[N];
	int8_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = g_random_int();

	int8_t dest[N];

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_16(void)
{
	enum { N = 256 };
	static const int16_t zero[N];
	int16_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = g_random_int();

	int16_t dest[N];

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

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
test_pcm_volume_24(void)
{
	enum { N = 256 };
	static const int32_t zero[N];
	int32_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = random24();

	int32_t dest[N];

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_32(void)
{
	enum { N = 256 };
	static const int32_t zero[N];
	int32_t src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = g_random_int();

	int32_t dest[N];

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_float(void)
{
	enum { N = 256 };
	static const float zero[N];
	float src[N];
	for (unsigned i = 0; i < N; ++i)
		src[i] = g_random_double_range(-1.0, 1.0);

	float dest[N];

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	memcpy(dest, src, sizeof(src));
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i)
		g_assert_cmpfloat(dest[i], ==, src[i] / 2);
}
