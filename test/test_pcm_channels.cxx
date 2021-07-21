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

#include "test_pcm_util.hxx"
#include "pcm/PcmChannels.hxx"
#include "pcm/Buffer.hxx"
#include "util/ConstBuffer.hxx"

#include <gtest/gtest.h>

TEST(PcmTest, Channels16)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N * 2>();

	PcmBuffer buffer;

	/* stereo to mono */

	auto dest = pcm_convert_channels_16(buffer, 1, 2, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N, dest.size);
	for (unsigned i = 0; i < N; ++i)
		EXPECT_EQ(int16_t((src[i * 2] + src[i * 2 + 1]) / 2),
			  dest[i]);

	/* mono to stereo */

	dest = pcm_convert_channels_16(buffer, 2, 1, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N * 4, dest.size);
	for (unsigned i = 0; i < N; ++i) {
		EXPECT_EQ(src[i], dest[i * 2]);
		EXPECT_EQ(src[i], dest[i * 2 + 1]);
	}

	/* stereo to 5.1 */

	dest = pcm_convert_channels_16(buffer, 6, 2, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N * 6, dest.size);
	constexpr int16_t silence = 0;
	for (unsigned i = 0; i < N; ++i) {
		EXPECT_EQ(src[i * 2], dest[i * 6]);
		EXPECT_EQ(src[i * 2 + 1], dest[i * 6+ 1]);
		EXPECT_EQ(silence, dest[i * 6 + 2]);
		EXPECT_EQ(silence, dest[i * 6 + 3]);
		EXPECT_EQ(silence, dest[i * 6 + 4]);
		EXPECT_EQ(silence, dest[i * 6 + 5]);
	}
}

TEST(PcmTest, Channels32)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int32_t, N * 2>();

	PcmBuffer buffer;

	/* stereo to mono */

	auto dest = pcm_convert_channels_32(buffer, 1, 2, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N, dest.size);
	for (unsigned i = 0; i < N; ++i)
		EXPECT_EQ(int32_t(((int64_t)src[i * 2] + (int64_t)src[i * 2 + 1]) / 2),
			  dest[i]);

	/* mono to stereo */

	dest = pcm_convert_channels_32(buffer, 2, 1, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N * 4, dest.size);
	for (unsigned i = 0; i < N; ++i) {
		EXPECT_EQ(src[i], dest[i * 2]);
		EXPECT_EQ(src[i], dest[i * 2 + 1]);
	}

	/* stereo to 5.1 */

	dest = pcm_convert_channels_32(buffer, 6, 2, { src, N * 2 });
	EXPECT_FALSE(dest.IsNull());
	EXPECT_EQ(N * 6, dest.size);
	constexpr int32_t silence = 0;
	for (unsigned i = 0; i < N; ++i) {
		EXPECT_EQ(src[i * 2], dest[i * 6]);
		EXPECT_EQ(src[i * 2 + 1], dest[i * 6+ 1]);
		EXPECT_EQ(silence, dest[i * 6 + 2]);
		EXPECT_EQ(silence, dest[i * 6 + 3]);
		EXPECT_EQ(silence, dest[i * 6 + 4]);
		EXPECT_EQ(silence, dest[i * 6 + 5]);
	}
}
