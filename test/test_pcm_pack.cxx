// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "test_pcm_util.hxx"
#include "pcm/Pack.hxx"
#include "util/ByteOrder.hxx"

#include <gtest/gtest.h>

TEST(PcmTest, Pack24)
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<int32_t, N>(RandomInt24());

	uint8_t dest[N * 3];
	pcm_pack_24(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		int32_t d;
		if (IsBigEndian())
			d = (dest[i * 3] << 16) | (dest[i * 3 + 1] << 8)
				| dest[i * 3 + 2];
		else
			d = (dest[i * 3 + 2] << 16) | (dest[i * 3 + 1] << 8)
				| dest[i * 3];
		if (d & 0x800000)
			d |= 0xff000000;

		EXPECT_EQ(d, src[i]);
	}
}

TEST(PcmTest, Unpack24)
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<uint8_t, N * 3>();

	int32_t dest[N];
	pcm_unpack_24(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		int32_t s;
		if (IsBigEndian())
			s = (src[i * 3] << 16) | (src[i * 3 + 1] << 8)
				| src[i * 3 + 2];
		else
			s = (src[i * 3 + 2] << 16) | (src[i * 3 + 1] << 8)
				| src[i * 3];
		if (s & 0x800000)
			s |= 0xff000000;

		EXPECT_EQ(s, dest[i]);
	}
}

TEST(PcmTest, Unpack24BE)
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<uint8_t, N * 3>();

	int32_t dest[N];
	pcm_unpack_24be(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		int32_t s;
		s = (src[i * 3] << 16) | (src[i * 3 + 1] << 8)
			| src[i * 3 + 2];
		if (s & 0x800000)
			s |= 0xff000000;

		EXPECT_EQ(s, dest[i]);
	}
}
