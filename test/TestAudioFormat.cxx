// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
