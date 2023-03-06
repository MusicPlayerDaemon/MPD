// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "tag/MixRampParser.cxx"
#include "tag/MixRampInfo.hxx"

#include <gtest/gtest.h>

TEST(MixRamp, Parser)
{
	MixRampInfo info;
	EXPECT_FALSE(info.IsDefined());
	EXPECT_EQ(info.GetStart(), nullptr);
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_FALSE(ParseMixRampTag(info, "foo", "bar"));
	EXPECT_FALSE(info.IsDefined());
	EXPECT_EQ(info.GetStart(), nullptr);
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampTag(info, "mixramp_start", "foo"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "foo");
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampTag(info, "MIXRAMP_START", "bar"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampTag(info, "mixramp_end", "abc"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_STREQ(info.GetEnd(), "abc");

	EXPECT_TRUE(ParseMixRampTag(info, "MIXRAMP_END", "def"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_STREQ(info.GetEnd(), "def");
}

TEST(MixRamp, VorbisParser)
{
	MixRampInfo info;
	EXPECT_FALSE(info.IsDefined());
	EXPECT_EQ(info.GetStart(), nullptr);
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_FALSE(ParseMixRampVorbis(info, "foo=bar"));
	EXPECT_FALSE(info.IsDefined());
	EXPECT_EQ(info.GetStart(), nullptr);
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampVorbis(info, "mixramp_start=foo"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "foo");
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampVorbis(info, "MIXRAMP_START=bar"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_EQ(info.GetEnd(), nullptr);

	EXPECT_TRUE(ParseMixRampVorbis(info, "mixramp_end=abc"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_STREQ(info.GetEnd(), "abc");

	EXPECT_TRUE(ParseMixRampVorbis(info, "MIXRAMP_END=def"));
	EXPECT_TRUE(info.IsDefined());
	EXPECT_STREQ(info.GetStart(), "bar");
	EXPECT_STREQ(info.GetEnd(), "def");
}
