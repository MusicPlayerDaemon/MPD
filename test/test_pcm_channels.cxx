/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "test_pcm_all.hxx"
#include "test_pcm_util.hxx"
#include "pcm/PcmChannels.hxx"
#include "pcm/PcmBuffer.hxx"
#include "util/ConstBuffer.hxx"

void
PcmChannelsTest::TestChannels16()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N * 2>();

	PcmBuffer buffer;

	/* stereo to mono */

	auto dest = pcm_convert_channels_16(buffer, 1, 2, { src, N * 2 });
	CPPUNIT_ASSERT(!dest.IsNull());
	CPPUNIT_ASSERT_EQUAL(N, dest.size);
	for (unsigned i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(int16_t((src[i * 2] + src[i * 2 + 1]) / 2),
				     dest[i]);

	/* mono to stereo */

	dest = pcm_convert_channels_16(buffer, 2, 1, { src, N * 2 });
	CPPUNIT_ASSERT(!dest.IsNull());
	CPPUNIT_ASSERT_EQUAL(N * 4, dest.size);
	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT_EQUAL(src[i], dest[i * 2]);
		CPPUNIT_ASSERT_EQUAL(src[i], dest[i * 2 + 1]);
	}
}

void
PcmChannelsTest::TestChannels32()
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int32_t, N * 2>();

	PcmBuffer buffer;

	/* stereo to mono */

	auto dest = pcm_convert_channels_32(buffer, 1, 2, { src, N * 2 });
	CPPUNIT_ASSERT(!dest.IsNull());
	CPPUNIT_ASSERT_EQUAL(N, dest.size);
	for (unsigned i = 0; i < N; ++i)
		CPPUNIT_ASSERT_EQUAL(int32_t(((int64_t)src[i * 2] + (int64_t)src[i * 2 + 1]) / 2),
				     dest[i]);

	/* mono to stereo */

	dest = pcm_convert_channels_32(buffer, 2, 1, { src, N * 2 });
	CPPUNIT_ASSERT(!dest.IsNull());
	CPPUNIT_ASSERT_EQUAL(N * 4, dest.size);
	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT_EQUAL(src[i], dest[i * 2]);
		CPPUNIT_ASSERT_EQUAL(src[i], dest[i * 2 + 1]);
	}
}
