/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "pcm/Interleave.hxx"
#include "util/Macros.hxx"

#include <algorithm>

template<typename T>
static void
TestInterleaveN()
{
	static constexpr T src1[] = { 1, 4, 7 };
	static constexpr T src2[] = { 2, 5, 8 };
	static constexpr T src3[] = { 3, 6, 9 };
	static constexpr const T *src_all[] = { src1, src2, src3 };

	static constexpr size_t n_frames = ARRAY_SIZE(src1);
	static constexpr unsigned channels = ARRAY_SIZE(src_all);

	static const ConstBuffer<const void *> src((const void *const*)src_all,
						   channels);

	static constexpr T poison = T(0xdeadbeef);
	T dest[n_frames * channels + 1];
	std::fill_n(dest, ARRAY_SIZE(dest), poison);

	PcmInterleave(dest, src, n_frames, sizeof(T));

	CPPUNIT_ASSERT_EQUAL(T(1), dest[0]);
	CPPUNIT_ASSERT_EQUAL(T(2), dest[1]);
	CPPUNIT_ASSERT_EQUAL(T(3), dest[2]);
	CPPUNIT_ASSERT_EQUAL(T(4), dest[3]);
	CPPUNIT_ASSERT_EQUAL(T(5), dest[4]);
	CPPUNIT_ASSERT_EQUAL(T(6), dest[5]);
	CPPUNIT_ASSERT_EQUAL(T(7), dest[6]);
	CPPUNIT_ASSERT_EQUAL(T(8), dest[7]);
	CPPUNIT_ASSERT_EQUAL(T(9), dest[8]);
	CPPUNIT_ASSERT_EQUAL(poison, dest[9]);
}

void
PcmInterleaveTest::TestInterleave8()
{
	TestInterleaveN<uint8_t>();
}

void
PcmInterleaveTest::TestInterleave16()
{
	TestInterleaveN<uint16_t>();
}

void
PcmInterleaveTest::TestInterleave24()
{
	typedef uint8_t T;
	static constexpr T src1[] = { 1, 2, 3, 4, 5, 6 };
	static constexpr T src2[] = { 7, 8, 9, 10, 11, 12 };
	static constexpr T src3[] = { 13, 14, 15, 16, 17, 18 };
	static constexpr const T *src_all[] = { src1, src2, src3 };

	static constexpr size_t n_frames = ARRAY_SIZE(src1) / 3;
	static constexpr unsigned channels = ARRAY_SIZE(src_all);

	static const ConstBuffer<const void *> src((const void *const*)src_all,
						   channels);

	static constexpr T poison = 0xff;
	T dest[n_frames * channels * 3 + 1];
	std::fill_n(dest, ARRAY_SIZE(dest), poison);

	PcmInterleave(dest, src, n_frames, 3);

	CPPUNIT_ASSERT_EQUAL(T(1), dest[0]);
	CPPUNIT_ASSERT_EQUAL(T(2), dest[1]);
	CPPUNIT_ASSERT_EQUAL(T(3), dest[2]);
	CPPUNIT_ASSERT_EQUAL(T(7), dest[3]);
	CPPUNIT_ASSERT_EQUAL(T(8), dest[4]);
	CPPUNIT_ASSERT_EQUAL(T(9), dest[5]);
	CPPUNIT_ASSERT_EQUAL(T(13), dest[6]);
	CPPUNIT_ASSERT_EQUAL(T(14), dest[7]);
	CPPUNIT_ASSERT_EQUAL(T(15), dest[8]);
	CPPUNIT_ASSERT_EQUAL(T(4), dest[9]);
	CPPUNIT_ASSERT_EQUAL(T(5), dest[10]);
	CPPUNIT_ASSERT_EQUAL(T(6), dest[11]);
	CPPUNIT_ASSERT_EQUAL(T(10), dest[12]);
	CPPUNIT_ASSERT_EQUAL(T(11), dest[13]);
	CPPUNIT_ASSERT_EQUAL(T(12), dest[14]);
	CPPUNIT_ASSERT_EQUAL(T(16), dest[15]);
	CPPUNIT_ASSERT_EQUAL(T(17), dest[16]);
	CPPUNIT_ASSERT_EQUAL(T(18), dest[17]);
	CPPUNIT_ASSERT_EQUAL(poison, dest[18]);

	TestInterleaveN<uint16_t>();
}

void
PcmInterleaveTest::TestInterleave32()
{
	TestInterleaveN<uint8_t>();
}

void
PcmInterleaveTest::TestInterleave64()
{
	TestInterleaveN<uint64_t>();
}
