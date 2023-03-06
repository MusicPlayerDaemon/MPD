// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "test_pcm_util.hxx"
#include "pcm/Dither.cxx"

#include <gtest/gtest.h>

TEST(PcmTest, Dither24)
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<int32_t, N>(RandomInt24());

	int16_t dest[N];
	PcmDither dither;
	dither.Dither24To16(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		EXPECT_GE(dest[i], (src[i] >> 8) - 8);
		EXPECT_LT(dest[i], (src[i] >> 8) + 8);
	}
}

TEST(PcmTest, Dither32)
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<int32_t, N>();

	int16_t dest[N];
	PcmDither dither;
	dither.Dither32To16(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		EXPECT_GE(dest[i], (src[i] >> 16) - 8);
		EXPECT_LT(dest[i], (src[i] >> 16) + 8);
	}
}
