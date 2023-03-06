// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "test_pcm_util.hxx"
#include "pcm/PcmFormat.hxx"
#include "pcm/Dither.hxx"
#include "pcm/Buffer.hxx"
#include "pcm/SampleFormat.hxx"

#include <gtest/gtest.h>

TEST(PcmTest, Format8To16)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int8_t, N>();

	PcmBuffer buffer;

	PcmDither dither;
	auto d = pcm_convert_to_16(buffer, dither, SampleFormat::S8, src);
	EXPECT_EQ(N, d.size());

	for (size_t i = 0; i < N; ++i)
		EXPECT_EQ(int(src[i]), d[i] >> 8);
}

TEST(PcmTest, Format16To24)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer;

	auto d = pcm_convert_to_24(buffer, SampleFormat::S16, src);
	EXPECT_EQ(N, d.size());

	for (size_t i = 0; i < N; ++i)
		EXPECT_EQ(int(src[i]), d[i] >> 8);
}

TEST(PcmTest, Format16To32)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer;

	auto d = pcm_convert_to_32(buffer, SampleFormat::S16, src);
	EXPECT_EQ(N, d.size());

	for (size_t i = 0; i < N; ++i)
		EXPECT_EQ(int(src[i]), d[i] >> 16);
}

TEST(PcmTest, FormatFloat16)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int16_t, N>();

	PcmBuffer buffer1, buffer2;

	auto f = pcm_convert_to_float(buffer1, SampleFormat::S16, src);
	EXPECT_EQ(N, f.size());

	for (size_t i = 0; i != f.size(); ++i) {
		EXPECT_GE(f[i], -1.f);
		EXPECT_LE(f[i], 1.f);
	}

	PcmDither dither;

	auto d = pcm_convert_to_16(buffer2, dither,
				   SampleFormat::FLOAT,
				   std::as_bytes(f));
	EXPECT_EQ(N, d.size());

	for (size_t i = 0; i < N; ++i)
		EXPECT_EQ(src[i], d[i]);

	/* check if clamping works */
	auto *writable = const_cast<float *>(f.data());
	*writable++ = 1.01;
	*writable++ = 10;
	*writable++ = -1.01;
	*writable++ = -10;

	d = pcm_convert_to_16(buffer2, dither,
			      SampleFormat::FLOAT,
			      std::as_bytes(f));
	EXPECT_EQ(N, d.size());

	EXPECT_EQ(32767, int(d[0]));
	EXPECT_EQ(32767, int(d[1]));
	EXPECT_EQ(-32768, int(d[2]));
	EXPECT_EQ(-32768, int(d[3]));

	for (size_t i = 4; i < N; ++i)
		EXPECT_EQ(src[i], d[i]);
}

TEST(PcmTest, FormatFloat32)
{
	constexpr size_t N = 509;
	const auto src = TestDataBuffer<int32_t, N>();

	PcmBuffer buffer1, buffer2;

	auto f = pcm_convert_to_float(buffer1, SampleFormat::S32, src);
	EXPECT_EQ(N, f.size());

	for (size_t i = 0; i != f.size(); ++i) {
		EXPECT_GE(f[i], -1.f);
		EXPECT_LE(f[i], 1.f);
	}

	auto d = pcm_convert_to_32(buffer2,
				   SampleFormat::FLOAT,
				   std::as_bytes(f));
	EXPECT_EQ(N, d.size());

	constexpr int error = 64;

	for (size_t i = 0; i < N; ++i)
		EXPECT_NEAR(src[i], d[i], error);

	/* check if clamping works */
	auto *writable = const_cast<float *>(f.data());
	*writable++ = 1.01;
	*writable++ = 10;
	*writable++ = -1.01;
	*writable++ = -10;

	d = pcm_convert_to_32(buffer2,
			      SampleFormat::FLOAT,
			      std::as_bytes(f));
	EXPECT_EQ(N, d.size());

	EXPECT_EQ(2147483647, int(d[0]));
	EXPECT_EQ(2147483647, int(d[1]));
	EXPECT_EQ(-2147483648, int(d[2]));
	EXPECT_EQ(-2147483648, int(d[3]));

	for (size_t i = 4; i < N; ++i)
		EXPECT_NEAR(src[i], d[i], error);
}
