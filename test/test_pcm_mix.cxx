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
#include "pcm/Mix.hxx"
#include "pcm/Dither.hxx"

#include <gtest/gtest.h>

template<typename T, SampleFormat format, typename G=RandomInt<T>>
static void
TestPcmMix(G g=G())
{
	constexpr unsigned N = 509;
	const auto src1 = TestDataBuffer<T, N>(g);
	const auto src2 = TestDataBuffer<T, N>(g);

	PcmDither dither;

	/* portion1=1.0: result must be equal to src1 */
	auto result = src1;
	bool success = pcm_mix(dither,
			       result.begin(), src2.begin(), sizeof(result),
			       format, 1.0);
	ASSERT_TRUE(success);
	AssertEqualWithTolerance(result, src1, 3);

	/* portion1=0.0: result must be equal to src2 */
	result = src1;
	success = pcm_mix(dither, result.begin(), src2.begin(), sizeof(result),
			  format, 0.0);
	ASSERT_TRUE(success);
	AssertEqualWithTolerance(result, src2, 3);

	/* portion1=0.5 */
	result = src1;
	success = pcm_mix(dither, result.begin(), src2.begin(), sizeof(result),
			  format, 0.5);
	ASSERT_TRUE(success);

	auto expected = src1;
	for (unsigned i = 0; i < N; ++i)
		expected[i] = (int64_t(src1[i]) + int64_t(src2[i])) / 2;

	for (unsigned i = 0; i < N; ++i)
		EXPECT_NEAR(result[i], expected[i], 3);
}

TEST(PcmTest, Mix8)
{
	TestPcmMix<int8_t, SampleFormat::S8>();
}

TEST(PcmTest, Mix16)
{
	TestPcmMix<int16_t, SampleFormat::S16>();
}

TEST(PcmTest, Mix24)
{
	TestPcmMix<int32_t, SampleFormat::S24_P32>(RandomInt24());
}

TEST(PcmTest, Mix32)
{
	TestPcmMix<int32_t, SampleFormat::S32>();
}
