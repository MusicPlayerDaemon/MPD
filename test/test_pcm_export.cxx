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
#include "pcm/PcmExport.hxx"
#include "system/ByteOrder.hxx"
#include "util/ConstBuffer.hxx"

#include <string.h>

void
PcmExportTest::TestShift8()
{
	static constexpr int32_t src[] = { 0x0, 0x1, 0x100, 0x10000, 0xffffff };
	static constexpr uint32_t expected[] = { 0x0, 0x100, 0x10000, 0x1000000, 0xffffff00 };

	PcmExport e;
	e.Open(SampleFormat::S24_P32, 2, false, true, false, false);

	auto dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(sizeof(expected), dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, expected, dest.size) == 0);
}

void
PcmExportTest::TestPack24()
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

	PcmExport e;
	e.Open(SampleFormat::S24_P32, 2, false, false, true, false);

	auto dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(expected_size, dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, expected, dest.size) == 0);
}

void
PcmExportTest::TestReverseEndian()
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

	PcmExport e;
	e.Open(SampleFormat::S8, 2, false, false, false, true);

	auto dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(sizeof(src), dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, src, dest.size) == 0);

	e.Open(SampleFormat::S16, 2, false, false, false, true);
	dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(sizeof(expected2), dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, expected2, dest.size) == 0);

	e.Open(SampleFormat::S32, 2, false, false, false, true);
	dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(sizeof(expected4), dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, expected4, dest.size) == 0);
}

void
PcmExportTest::TestDop()
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

	PcmExport e;
	e.Open(SampleFormat::DSD, 2, true, false, false, false);

	auto dest = e.Export({src, sizeof(src)});
	CPPUNIT_ASSERT_EQUAL(sizeof(expected), dest.size);
	CPPUNIT_ASSERT(memcmp(dest.data, expected, dest.size) == 0);
}
