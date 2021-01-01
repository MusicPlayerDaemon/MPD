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

#include "config.h"
#include "pcm/Export.hxx"
#include "pcm/Traits.hxx"
#include "util/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"

#include <gtest/gtest.h>

#include <string.h>

TEST(PcmTest, ExportShift8)
{
	static constexpr int32_t src[] = { 0x0, 0x1, 0x100, 0x10000, 0xffffff };
	static constexpr uint32_t expected[] = { 0x0, 0x100, 0x10000, 0x1000000, 0xffffff00 };

	PcmExport::Params params;
	params.shift8 = true;

	EXPECT_EQ(params.CalcOutputSampleRate(42u), 42u);
	EXPECT_EQ(params.CalcInputSampleRate(42u), 42u);

	PcmExport e;
	e.Open(SampleFormat::S24_P32, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 8u);
	EXPECT_EQ(e.GetOutputFrameSize(), 8u);
	EXPECT_EQ(e.GetInputBlockSize(), 8u);
	EXPECT_EQ(e.GetOutputBlockSize(), 8u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint8_t expected_silence[8]{};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

TEST(PcmTest, ExportPack24)
{
	static constexpr int32_t src[] = { 0x0, 0x1, 0x100, 0x10000, 0xffffff };

	static constexpr uint8_t expected_be[] = {
		0, 0, 0x0,
		0, 0, 0x1,
		0, 0x1, 0x00,
		0x1, 0x00, 0x00,
		0xff, 0xff, 0xff,
	};

	static constexpr uint8_t expected_le[] = {
		0, 0, 0x0,
		0x1, 0, 0,
		0x00, 0x1, 0,
		0, 0x00, 0x01,
		0xff, 0xff, 0xff,
	};

	static constexpr size_t expected_size = sizeof(expected_be);
	static const uint8_t *const expected = IsBigEndian()
		? expected_be : expected_le;

	PcmExport::Params params;
	params.pack24 = true;

	EXPECT_EQ(params.CalcOutputSampleRate(42u), 42u);
	EXPECT_EQ(params.CalcInputSampleRate(42u), 42u);

	PcmExport e;
	e.Open(SampleFormat::S24_P32, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 8u);
	EXPECT_EQ(e.GetOutputFrameSize(), 6u);
	EXPECT_EQ(e.GetInputBlockSize(), 8u);
	EXPECT_EQ(e.GetOutputBlockSize(), 6u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(expected_size, dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint8_t expected_silence[6]{};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

TEST(PcmTest, ExportReverseEndian)
{
	static constexpr uint8_t src[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
	};

	static constexpr uint8_t expected2[] = {
		2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11
	};

	static constexpr uint8_t expected4[] = {
		4, 3, 2, 1, 8, 7, 6, 5, 12, 11, 10, 9,
	};

	PcmExport::Params params;
	params.reverse_endian = true;

	EXPECT_EQ(params.CalcOutputSampleRate(42u), 42u);
	EXPECT_EQ(params.CalcInputSampleRate(42u), 42u);

	PcmExport e;
	e.Open(SampleFormat::S8, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 2u);
	EXPECT_EQ(e.GetOutputFrameSize(), 2u);
	EXPECT_EQ(e.GetInputBlockSize(), 2u);
	EXPECT_EQ(e.GetOutputBlockSize(), 2u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(src), dest.size);
	EXPECT_TRUE(memcmp(dest.data, src, dest.size) == 0);

	e.Open(SampleFormat::S16, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 4u);
	EXPECT_EQ(e.GetOutputFrameSize(), 4u);
	EXPECT_EQ(e.GetInputBlockSize(), 4u);
	EXPECT_EQ(e.GetOutputBlockSize(), 4u);

	dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected2), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected2, dest.size) == 0);

	e.Open(SampleFormat::S32, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 8u);
	EXPECT_EQ(e.GetOutputFrameSize(), 8u);
	EXPECT_EQ(e.GetInputBlockSize(), 8u);
	EXPECT_EQ(e.GetOutputBlockSize(), 8u);

	dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected4), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected4, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint8_t expected_silence[8]{};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

#ifdef ENABLE_DSD

TEST(PcmTest, ExportDsdU16)
{
	static constexpr uint8_t src[] = {
		0x01, 0x23, 0x45, 0x67,
		0x89, 0xab, 0xcd, 0xef,
		0x11, 0x22, 0x33, 0x44,
		0x55, 0x66, 0x77, 0x88,
	};

	static constexpr uint16_t expected[] = {
		0x0145, 0x2367,
		0x89cd, 0xabef,
		0x1133, 0x2244,
		0x5577, 0x6688,
	};

	PcmExport::Params params;
	params.dsd_mode = PcmExport::DsdMode::U16;

	EXPECT_EQ(params.CalcOutputSampleRate(705600u), 352800u);
	EXPECT_EQ(params.CalcInputSampleRate(352800u), 705600u);

	PcmExport e;
	e.Open(SampleFormat::DSD, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 2u);
	EXPECT_EQ(e.GetOutputFrameSize(), 4u);
	EXPECT_EQ(e.GetInputBlockSize(), 4u);
	EXPECT_EQ(e.GetOutputBlockSize(), 4u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	/* no output, 2/4 remains */
	static constexpr uint8_t src2[] = { 0x11, 0x22 };
	dest = e.Export({src2, sizeof(src2)});
	EXPECT_TRUE(dest.empty());

	/* one full frame and 2/4 remains */
	static constexpr uint8_t src3[] = { 0x33, 0x44, 0x55, 0x66 };
	static constexpr uint16_t expected3[] = { 0x1133, 0x2244 };
	dest = e.Export({src3, sizeof(src3)});
	EXPECT_EQ(sizeof(expected3), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected3, dest.size) == 0);

	/* two full frames and 2/4 remains again */
	static constexpr uint8_t src4[] = { 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee };
	static constexpr uint16_t expected4[] = { 0x5577, 0x6688, 0x99bb, 0xaacc };
	dest = e.Export({src4, sizeof(src4)});
	EXPECT_EQ(sizeof(expected4), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected4, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint8_t expected_silence[]{0x69, 0x69, 0x69, 0x69};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

TEST(PcmTest, ExportDsdU32)
{
	static constexpr uint8_t src[] = {
		0x01, 0x23, 0x45, 0x67,
		0x89, 0xab, 0xcd, 0xef,
		0x11, 0x22, 0x33, 0x44,
		0x55, 0x66, 0x77, 0x88,
	};

	static constexpr uint32_t expected[] = {
		0x014589cd,
		0x2367abef,
		0x11335577,
		0x22446688,
	};

	PcmExport::Params params;
	params.dsd_mode = PcmExport::DsdMode::U32;

	EXPECT_EQ(params.CalcOutputSampleRate(705600u), 176400u);
	EXPECT_EQ(params.CalcInputSampleRate(176400u), 705600u);

	PcmExport e;
	e.Open(SampleFormat::DSD, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 2u);
	EXPECT_EQ(e.GetOutputFrameSize(), 8u);
	EXPECT_EQ(e.GetInputBlockSize(), 8u);
	EXPECT_EQ(e.GetOutputBlockSize(), 8u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	/* no output, 4/8 remains */
	static constexpr uint8_t src2[] = { 0x11, 0x22, 0x33, 0x44 };
	dest = e.Export({src2, sizeof(src2)});
	EXPECT_TRUE(dest.empty());

	/* one full frame and 4/8 remains */
	static constexpr uint8_t src3[] = { 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc };
	static constexpr uint32_t expected3[] = { 0x11335577, 0x22446688 };
	dest = e.Export({src3, sizeof(src3)});
	EXPECT_EQ(sizeof(expected3), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected3, dest.size) == 0);

	/* two full frames and 2/4 remains again */
	static constexpr uint8_t src4[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
	static constexpr uint32_t expected4[] = { 0x99bb0103, 0xaacc0204 };
	dest = e.Export({src4, sizeof(src4)});
	EXPECT_EQ(sizeof(expected4), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected4, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint8_t expected_silence[]{0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69, 0x69};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

TEST(PcmTest, ExportDop)
{
	static constexpr uint8_t src[] = {
		0x01, 0x23, 0x45, 0x67,
		0x89, 0xab, 0xcd, 0xef,
	};

	static constexpr uint32_t expected[] = {
		0xff050145,
		0xff052367,
		0xfffa89cd,
		0xfffaabef,
	};

	PcmExport::Params params;
	params.dsd_mode = PcmExport::DsdMode::DOP;

	EXPECT_EQ(params.CalcOutputSampleRate(705600u), 352800u);
	EXPECT_EQ(params.CalcInputSampleRate(352800u), 705600u);

	PcmExport e;
	e.Open(SampleFormat::DSD, 2, params);

	EXPECT_EQ(e.GetInputFrameSize(), 2u);
	EXPECT_EQ(e.GetOutputFrameSize(), 8u);
	EXPECT_EQ(e.GetInputBlockSize(), 8u);
	EXPECT_EQ(e.GetOutputBlockSize(), 16u);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	/* not enough data: 2/8 */
	static constexpr uint8_t src2[] = { 0x12, 0x34 };
	dest = e.Export({src2, sizeof(src2)});
	ASSERT_EQ(dest.size, 0u);

	/* not enough data: 6/8 */
	static constexpr uint8_t src3[] = { 0x56, 0x78, 0x9a, 0xbc };
	dest = e.Export({src3, sizeof(src3)});
	ASSERT_EQ(dest.size, 0u);

	/* just enough data: 8/8 */
	static constexpr uint8_t src4[] = { 0xde, 0xf0 };
	static constexpr uint32_t expected4[] = { 0xff051256, 0xff053478, 0xfffa9ade, 0xfffabcf0 };
	dest = e.Export({src4, sizeof(src4)});
	ASSERT_EQ(sizeof(expected4), dest.size);
	ASSERT_TRUE(memcmp(dest.data, expected4, dest.size) == 0);

	/* not enough data: 6/8 */
	static constexpr uint8_t src5[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
	dest = e.Export({src5, sizeof(src5)});
	ASSERT_TRUE(dest.empty());

	/* two quads returned, not enough data for more: 2/8 */
	static constexpr uint8_t src6[] = { 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x10, 0x20 };
	static constexpr uint32_t expected6[] = { 0xff051133, 0xff052244, 0xfffa5577, 0xfffa6688, 0xff0599bb, 0xff05aacc, 0xfffaddff, 0xfffaee00 };
	dest = e.Export({src6, sizeof(src6)});
	ASSERT_EQ(sizeof(expected6), dest.size);
	ASSERT_TRUE(memcmp(dest.data, expected6, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr uint32_t expected_silence[]{0xff056969, 0xff056969, 0xfffa6969, 0xfffa6969};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

#endif

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
TestAlsaChannelOrder51()
{
	using value_type = typename Traits::value_type;

	static constexpr value_type src[] = {
		0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 10, 11,
	};

	static constexpr value_type expected[] = {
		0, 1, 4, 5, 2, 3,
		6, 7, 10, 11, 8, 9,
	};

	PcmExport::Params params;
	params.alsa_channel_order = true;

	EXPECT_EQ(params.CalcOutputSampleRate(42u), 42u);
	EXPECT_EQ(params.CalcInputSampleRate(42u), 42u);

	PcmExport e;
	e.Open(F, 6, params);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
TestAlsaChannelOrder71()
{
	using value_type = typename Traits::value_type;

	static constexpr value_type src[] = {
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15,
	};

	static constexpr value_type expected[] = {
		0, 1, 4, 5, 2, 3, 6, 7,
		8, 9, 12, 13, 10, 11, 14, 15,
	};

	PcmExport::Params params;
	params.alsa_channel_order = true;

	EXPECT_EQ(params.CalcOutputSampleRate(42u), 42u);
	EXPECT_EQ(params.CalcInputSampleRate(42u), 42u);

	PcmExport e;
	e.Open(F, 8, params);

	auto dest = e.Export({src, sizeof(src)});
	EXPECT_EQ(sizeof(expected), dest.size);
	EXPECT_TRUE(memcmp(dest.data, expected, dest.size) == 0);

	const auto silence = e.GetSilence();
	constexpr value_type expected_silence[8]{};
	EXPECT_EQ(silence.size, sizeof(expected_silence));
	EXPECT_EQ(memcmp(silence.data, expected_silence,
			 sizeof(expected_silence)), 0);
}

TEST(PcmTest, ExportAlsaChannelOrder)
{
	TestAlsaChannelOrder51<SampleFormat::S16>();
	TestAlsaChannelOrder71<SampleFormat::S16>();
	TestAlsaChannelOrder51<SampleFormat::S32>();
	TestAlsaChannelOrder71<SampleFormat::S32>();
}
