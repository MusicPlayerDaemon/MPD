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
#include "pcm/PcmVolume.hxx"
#include "test_pcm_util.hxx"

#include <algorithm>

#include <string.h>

void
PcmVolumeTest::TestVolume8()
{
	constexpr unsigned N = 256;
	static int8_t zero[N];
	const auto src = TestDataBuffer<int8_t, N>();

	int8_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S8, 0));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, zero, sizeof(zero)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S8, PCM_VOLUME_1));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, src, sizeof(src)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S8, PCM_VOLUME_1 / 2));

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] - 1) / 2);
		CPPUNIT_ASSERT(dest[i] <= src[i] / 2 + 1);
	}
}

void
PcmVolumeTest::TestVolume16()
{
	constexpr unsigned N = 256;
	static int16_t zero[N];
	const auto src = TestDataBuffer<int16_t, N>();

	int16_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S16, 0));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, zero, sizeof(zero)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S16, PCM_VOLUME_1));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, src, sizeof(src)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S16, PCM_VOLUME_1 / 2));

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] - 1) / 2);
		CPPUNIT_ASSERT(dest[i] <= src[i] / 2 + 1);
	}
}

void
PcmVolumeTest::TestVolume24()
{
	constexpr unsigned N = 256;
	static int32_t zero[N];
	const auto src = TestDataBuffer<int32_t, N>(RandomInt24());

	int32_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S24_P32, 0));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, zero, sizeof(zero)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S24_P32, PCM_VOLUME_1));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, src, sizeof(src)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S24_P32, PCM_VOLUME_1 / 2));

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] - 1) / 2);
		CPPUNIT_ASSERT(dest[i] <= src[i] / 2 + 1);
	}
}

void
PcmVolumeTest::TestVolume32()
{
	constexpr unsigned N = 256;
	static int32_t zero[N];
	const auto src = TestDataBuffer<int32_t, N>();

	int32_t dest[N];

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S32, 0));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, zero, sizeof(zero)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S32, PCM_VOLUME_1));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, src, sizeof(src)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::S32, PCM_VOLUME_1 / 2));

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] - 1) / 2);
		CPPUNIT_ASSERT(dest[i] <= src[i] / 2 + 1);
	}
}

void
PcmVolumeTest::TestVolumeFloat()
{
	constexpr unsigned N = 256;
	static float zero[N];
	const auto src = TestDataBuffer<float, N>(RandomFloat());

	float dest[N];

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::FLOAT, 0));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, zero, sizeof(zero)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::FLOAT, PCM_VOLUME_1));
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest, src, sizeof(src)));

	std::copy(src.begin(), src.end(), dest);
	CPPUNIT_ASSERT_EQUAL(true,
			     pcm_volume(dest, sizeof(dest),
					SampleFormat::FLOAT,
					PCM_VOLUME_1 / 2));

	for (unsigned i = 0; i < N; ++i)
		CPPUNIT_ASSERT_DOUBLES_EQUAL(src[i] / 2, dest[i], 1);
}
