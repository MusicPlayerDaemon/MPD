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

#ifndef MPD_TEST_PCM_ALL_HXX
#define MPD_TEST_PCM_ALL_HXX

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class PcmDitherTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmDitherTest);
	CPPUNIT_TEST(TestDither24);
	CPPUNIT_TEST(TestDither32);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestDither24();
	void TestDither32();
};

class PcmPackTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmPackTest);
	CPPUNIT_TEST(TestPack24);
	CPPUNIT_TEST(TestUnpack24);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestPack24();
	void TestUnpack24();
};

class PcmChannelsTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmChannelsTest);
	CPPUNIT_TEST(TestChannels16);
	CPPUNIT_TEST(TestChannels32);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestChannels16();
	void TestChannels32();
};

class PcmVolumeTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmVolumeTest);
	CPPUNIT_TEST(TestVolume8);
	CPPUNIT_TEST(TestVolume16);
	CPPUNIT_TEST(TestVolume24);
	CPPUNIT_TEST(TestVolume32);
	CPPUNIT_TEST(TestVolumeFloat);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestVolume8();
	void TestVolume16();
	void TestVolume24();
	void TestVolume32();
	void TestVolumeFloat();
};

class PcmFormatTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmFormatTest);
	CPPUNIT_TEST(TestFormat8to16);
	CPPUNIT_TEST(TestFormat16to24);
	CPPUNIT_TEST(TestFormat16to32);
	CPPUNIT_TEST(TestFormatFloat);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestFormat8to16();
	void TestFormat16to24();
	void TestFormat16to32();
	void TestFormatFloat();
};

class PcmMixTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmMixTest);
	CPPUNIT_TEST(TestMix8);
	CPPUNIT_TEST(TestMix16);
	CPPUNIT_TEST(TestMix24);
	CPPUNIT_TEST(TestMix32);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestMix8();
	void TestMix16();
	void TestMix24();
	void TestMix32();
};

class PcmExportTest : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(PcmExportTest);
	CPPUNIT_TEST(TestShift8);
	CPPUNIT_TEST(TestPack24);
	CPPUNIT_TEST(TestReverseEndian);
	CPPUNIT_TEST(TestDop);
	CPPUNIT_TEST_SUITE_END();

public:
	void TestShift8();
	void TestPack24();
	void TestReverseEndian();
	void TestDop();
};

#endif
