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
#include "pcm/Volume.hxx"
#include "pcm/Traits.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "test_pcm_util.hxx"

#include <algorithm>

#include <string.h>

template<SampleFormat F, class Traits=SampleTraits<F>,
	 typename G=RandomInt<typename Traits::value_type>>
static void
TestVolume(G g=G())
{
	typedef typename Traits::value_type value_type;

	PcmVolume pv;
	CPPUNIT_ASSERT(pv.Open(F, IgnoreError()));

	constexpr size_t N = 509;
	static value_type zero[N];
	const auto _src = TestDataBuffer<value_type, N>(g);
	const ConstBuffer<void> src(_src, sizeof(_src));

	pv.SetVolume(0);
	auto dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest.data, zero, sizeof(zero)));

	pv.SetVolume(PCM_VOLUME_1);
	dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest.data, src.data, src.size));

	pv.SetVolume(PCM_VOLUME_1 / 2);
	dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);

	const auto _dest = ConstBuffer<value_type>::FromVoid(dest);
	for (unsigned i = 0; i < N; ++i) {
		const auto expected = (_src[i] + 1) / 2;
		CPPUNIT_ASSERT(_dest[i] >= expected - 4);
		CPPUNIT_ASSERT(_dest[i] <= expected + 4);
	}

	pv.Close();
}

void
PcmVolumeTest::TestVolume8()
{
	TestVolume<SampleFormat::S8>();
}

void
PcmVolumeTest::TestVolume16()
{
	TestVolume<SampleFormat::S16>();
}

void
PcmVolumeTest::TestVolume24()
{
	TestVolume<SampleFormat::S24_P32>(RandomInt24());
}

void
PcmVolumeTest::TestVolume32()
{
	TestVolume<SampleFormat::S32>();
}

void
PcmVolumeTest::TestVolumeFloat()
{
	PcmVolume pv;
	CPPUNIT_ASSERT(pv.Open(SampleFormat::FLOAT, IgnoreError()));

	constexpr size_t N = 509;
	static float zero[N];
	const auto _src = TestDataBuffer<float, N>(RandomFloat());
	const ConstBuffer<void> src(_src, sizeof(_src));

	pv.SetVolume(0);
	auto dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest.data, zero, sizeof(zero)));

	pv.SetVolume(PCM_VOLUME_1);
	dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);
	CPPUNIT_ASSERT_EQUAL(0, memcmp(dest.data, src.data, src.size));

	pv.SetVolume(PCM_VOLUME_1 / 2);
	dest = pv.Apply(src);
	CPPUNIT_ASSERT_EQUAL(src.size, dest.size);

	const auto _dest = ConstBuffer<float>::FromVoid(dest);
	for (unsigned i = 0; i < N; ++i)
		CPPUNIT_ASSERT_DOUBLES_EQUAL(_src[i] / 2, _dest[i], 1);

	pv.Close();
}
