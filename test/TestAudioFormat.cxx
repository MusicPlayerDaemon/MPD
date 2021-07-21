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

#include "pcm/AudioFormat.hxx"
#include "pcm/AudioParser.hxx"
#include "util/StringBuffer.hxx"

#include <gtest/gtest.h>

#include <string.h>

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

TEST(AudioFormatTest, ToString)
{
	for (const auto &i : af_string_tests)
		EXPECT_STREQ(i.s, ToString(i.af).c_str());
}

TEST(AudioFormatTest, Parse)
{
	for (const auto &i : af_string_tests) {
		EXPECT_EQ(i.af, ParseAudioFormat(i.s, false));
		EXPECT_EQ(i.af, ParseAudioFormat(i.s, true));
	}

	for (const auto &i : af_mask_tests)
		EXPECT_EQ(i.af, ParseAudioFormat(i.s, true));
}
