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

#include "tag/MixRampParser.cxx"
#include "tag/MixRampInfo.hxx"
#include "util/StringView.hxx"

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
