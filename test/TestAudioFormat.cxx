/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "TestAudioFormat.hxx"
#include "AudioFormat.hxx"
#include "AudioParser.hxx"
#include "util/StringBuffer.hxx"

#include <cppunit/TestAssert.h>

#include <string.h>

namespace CppUnit {
template<>
struct assertion_traits<const char *>
{
	static bool equal(const char *x, const char *y)
	{
		return strcmp(x, y) == 0;
	}

	static std::string toString(const char *x)
	{
		return std::string("\"") + x + "\"";
	}
};

template<>
struct assertion_traits<AudioFormat>
{
	static bool equal(AudioFormat x, AudioFormat y)
	{
		return x == y;
	}

	static std::string toString(AudioFormat x)
	{
		return ToString(x).c_str();
	}
};
}

struct AudioFormatStringTest {
	AudioFormat af;
	const char *s;
};

static constexpr AudioFormatStringTest af_string_tests[] = {
	{ AudioFormat(44100, SampleFormat::S8, 1), "44100:8:1" },
	{ AudioFormat(44100, SampleFormat::S16, 2), "44100:16:2" },
	{ AudioFormat(48000, SampleFormat::S24_P32, 6), "48000:24:6" },
	{ AudioFormat(192000, SampleFormat::FLOAT, 2), "192000:f:2" },
	{ AudioFormat(352801, SampleFormat::DSD, 2), "352801:dsd:2" },
	{ AudioFormat(352800, SampleFormat::DSD, 2), "dsd64:2" },
};

static constexpr AudioFormatStringTest af_mask_tests[] = {
	{ AudioFormat(44100, SampleFormat::UNDEFINED, 1), "44100:*:1" },
	{ AudioFormat(44100, SampleFormat::S16, 0), "44100:16:*" },
	{ AudioFormat(0, SampleFormat::S24_P32, 6), "*:24:6" },
	{ AudioFormat::Undefined(), "*:*:*" },
};

void
AudioFormatTest::TestToString()
{
	for (const auto &i : af_string_tests)
		CPPUNIT_ASSERT_EQUAL(i.s, ToString(i.af).c_str());
}

void
AudioFormatTest::TestParse()
{
	for (const auto &i : af_string_tests) {
		CPPUNIT_ASSERT_EQUAL(i.af,
				     ParseAudioFormat(i.s, false));
		CPPUNIT_ASSERT_EQUAL(i.af,
				     ParseAudioFormat(i.s, true));
	}

	for (const auto &i : af_mask_tests)
		CPPUNIT_ASSERT_EQUAL(i.af,
				     ParseAudioFormat(i.s, true));
}
