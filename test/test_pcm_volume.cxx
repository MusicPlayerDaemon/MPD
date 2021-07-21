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

#include "pcm/Volume.hxx"
#include "pcm/Traits.hxx"
#include "util/ConstBuffer.hxx"
#include "test_pcm_util.hxx"

#include <gtest/gtest.h>

#include <algorithm>

#include <string.h>

template<SampleFormat F, class Traits=SampleTraits<F>,
	 typename G=RandomInt<typename Traits::value_type>>
static void
TestVolume(G g=G())
{
	using value_type = typename Traits::value_type;

	PcmVolume pv;
	EXPECT_EQ(pv.Open(F, false), F);

	constexpr size_t N = 509;
	static value_type zero[N];
	const auto _src = TestDataBuffer<value_type, N>(g);
	const ConstBuffer<void> src(_src, sizeof(_src));

	pv.SetVolume(0);
	auto dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);
	EXPECT_EQ(0, memcmp(dest.data, zero, sizeof(zero)));

	pv.SetVolume(PCM_VOLUME_1);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);
	EXPECT_EQ(0, memcmp(dest.data, src.data, src.size));

	pv.SetVolume(PCM_VOLUME_1 / 2);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);

	const auto _dest = ConstBuffer<value_type>::FromVoid(dest);
	for (unsigned i = 0; i < N; ++i) {
		const auto expected = (_src[i] + 1) / 2;
		EXPECT_GE(_dest[i], expected - 4);
		EXPECT_LE(_dest[i], expected + 4);
	}

	pv.Close();
}

TEST(PcmTest, Volume8)
{
	TestVolume<SampleFormat::S8>();
}

TEST(PcmTest, Volume16)
{
	TestVolume<SampleFormat::S16>();
}

TEST(PcmTest, Volume16to32)
{
	constexpr SampleFormat F = SampleFormat::S16;
	using value_type = int16_t;
	RandomInt<value_type> g;

	PcmVolume pv;
	EXPECT_EQ(pv.Open(F, true), SampleFormat::S24_P32);

	constexpr size_t N = 509;
	static value_type zero[N];
	const auto _src = TestDataBuffer<value_type, N>(g);
	const ConstBuffer<void> src(_src, sizeof(_src));

	pv.SetVolume(0);
	auto dest = pv.Apply(src);
	EXPECT_EQ(src.size * 2, dest.size);
	EXPECT_EQ(0, memcmp(dest.data, zero, sizeof(zero)));

	pv.SetVolume(PCM_VOLUME_1);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size * 2, dest.size);
	auto s = ConstBuffer<int16_t>::FromVoid(src);
	auto d = ConstBuffer<int32_t>::FromVoid(dest);
	for (size_t i = 0; i < N; ++i)
		EXPECT_EQ(d[i], s[i] << 8);

	pv.SetVolume(PCM_VOLUME_1 / 2);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size * 2, dest.size);

	s = ConstBuffer<int16_t>::FromVoid(src);
	d = ConstBuffer<int32_t>::FromVoid(dest);
	for (unsigned i = 0; i < N; ++i) {
		const int32_t expected = (s[i] << 8) / 2;
		EXPECT_EQ(d[i], expected);
	}

	pv.Close();
}

TEST(PcmTest, Volume24)
{
	TestVolume<SampleFormat::S24_P32>(RandomInt24());
}

TEST(PcmTest, Volume32)
{
	TestVolume<SampleFormat::S32>();
}

TEST(PcmTest, VolumeFloat)
{
	PcmVolume pv;
	pv.Open(SampleFormat::FLOAT, false);

	constexpr size_t N = 509;
	static float zero[N];
	const auto _src = TestDataBuffer<float, N>(RandomFloat());
	const ConstBuffer<void> src(_src, sizeof(_src));

	pv.SetVolume(0);
	auto dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);
	EXPECT_EQ(0, memcmp(dest.data, zero, sizeof(zero)));

	pv.SetVolume(PCM_VOLUME_1);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);
	EXPECT_EQ(0, memcmp(dest.data, src.data, src.size));

	pv.SetVolume(PCM_VOLUME_1 / 2);
	dest = pv.Apply(src);
	EXPECT_EQ(src.size, dest.size);

	const auto _dest = ConstBuffer<float>::FromVoid(dest);
	for (unsigned i = 0; i < N; ++i)
		EXPECT_NEAR((double)_src[i] / 2., (double)_dest[i], 1.);

	pv.Close();
}
