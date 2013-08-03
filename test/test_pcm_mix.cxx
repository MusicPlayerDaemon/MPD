/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "pcm/PcmMix.hxx"

#include <glib.h>

template<typename T, SampleFormat format, typename G=GlibRandomInt<T>>
void
TestPcmMix(G g=G())
{
	constexpr unsigned N = 256;
	const auto src1 = TestDataBuffer<T, N>(g);
	const auto src2 = TestDataBuffer<T, N>(g);

	/* portion1=1.0: result must be equal to src1 */
	auto result = src1;
	bool success = pcm_mix(result.begin(), src2.begin(), sizeof(result),
			       format, 1.0);
	g_assert(success);
	AssertEqualWithTolerance(result, src1, 1);

	/* portion1=0.0: result must be equal to src2 */
	result = src1;
	success = pcm_mix(result.begin(), src2.begin(), sizeof(result),
			  format, 0.0);
	g_assert(success);
	AssertEqualWithTolerance(result, src2, 1);

	/* portion1=0.5 */
	result = src1;
	success = pcm_mix(result.begin(), src2.begin(), sizeof(result),
			  format, 0.5);
	g_assert(success);

	auto expected = src1;
	for (unsigned i = 0; i < N; ++i)
		expected[i] = (int64_t(src1[i]) + int64_t(src2[i])) / 2;

	AssertEqualWithTolerance(result, expected, 1);
}

void
test_pcm_mix_8()
{
	TestPcmMix<int8_t, SampleFormat::S8>();
}

void
test_pcm_mix_16()
{
	TestPcmMix<int16_t, SampleFormat::S16>();
}

void
test_pcm_mix_24()
{
	TestPcmMix<int32_t, SampleFormat::S24_P32>(GlibRandomInt24());
}

void
test_pcm_mix_32()
{
	TestPcmMix<int32_t, SampleFormat::S32>();
}
