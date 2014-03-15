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

#include "test_pcm_all.hxx"
#include "test_pcm_util.hxx"
#include "pcm/PcmDither.cxx"

void
PcmDitherTest::TestDither24()
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<int32_t, N>(RandomInt24());

	int16_t dest[N];
	PcmDither dither;
	dither.Dither24To16(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] >> 8) - 8);
		CPPUNIT_ASSERT(dest[i] < (src[i] >> 8) + 8);
	}
}

void
PcmDitherTest::TestDither32()
{
	constexpr unsigned N = 509;
	const auto src = TestDataBuffer<int32_t, N>();

	int16_t dest[N];
	PcmDither dither;
	dither.Dither32To16(dest, src.begin(), src.end());

	for (unsigned i = 0; i < N; ++i) {
		CPPUNIT_ASSERT(dest[i] >= (src[i] >> 16) - 8);
		CPPUNIT_ASSERT(dest[i] < (src[i] >> 16) + 8);
	}
}
