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

#include "test_pcm_all.hxx"
#include "PcmVolume.hxx"
#include "test_pcm_util.hxx"

#include <glib.h>

#include <algorithm>

#include <string.h>

void
test_pcm_volume_8()
{
	constexpr unsigned N = 256;
	static int8_t zero[N];
	const auto src = TestDataBuffer<int8_t, N>();

	int8_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S8,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_16()
{
	constexpr unsigned N = 256;
	static int16_t zero[N];
	const auto src = TestDataBuffer<int16_t, N>();

	int16_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S16,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_24()
{
	constexpr unsigned N = 256;
	static int32_t zero[N];
	const auto src = TestDataBuffer<int32_t, N>(GlibRandomInt24());

	int32_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S24_P32,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_32()
{
	constexpr unsigned N = 256;
	static int32_t zero[N];
	const auto src = TestDataBuffer<int32_t, N>();

	int32_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_S32,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i) {
		g_assert_cmpint(dest[i], >=, (src[i] - 1) / 2);
		g_assert_cmpint(dest[i], <=, src[i] / 2 + 1);
	}
}

void
test_pcm_volume_float()
{
	constexpr unsigned N = 256;
	static float zero[N];
	const auto src = TestDataBuffer<float, N>(GlibRandomFloat());

	float dest[N];

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   0), ==, true);
	g_assert_cmpint(memcmp(dest, zero, sizeof(zero)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   PCM_VOLUME_1), ==, true);
	g_assert_cmpint(memcmp(dest, src, sizeof(src)), ==, 0);

	std::copy(src.begin(), src.end(), dest);
	g_assert_cmpint(pcm_volume(dest, sizeof(dest), SAMPLE_FORMAT_FLOAT,
				   PCM_VOLUME_1 / 2), ==, true);

	for (unsigned i = 0; i < N; ++i)
		g_assert_cmpfloat(dest[i], ==, src[i] / 2);
}
