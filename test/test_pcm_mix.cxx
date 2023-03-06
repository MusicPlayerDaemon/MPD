// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
