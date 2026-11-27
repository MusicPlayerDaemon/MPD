// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "protocol/ArgParser.hxx"
#include "protocol/Ack.hxx"
#include "protocol/RangeArg.hxx"
#include "Chrono.hxx"

#include <gtest/gtest.h>

TEST(ArgParser, U32)
{
	EXPECT_EQ(ParseCommandArgU32("0"), 0U);
	EXPECT_EQ(ParseCommandArgU32("42"), 42U);
	EXPECT_EQ(ParseCommandArgU32("4294967295"), 4294967295U);

	EXPECT_THROW(ParseCommandArgU32(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("abc"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("1x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("1.5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("-1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("+1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32(" 1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("4294967296"), ProtocolError);
	EXPECT_THROW(ParseCommandArgU32("99999999999999999999"), ProtocolError);
}

TEST(ArgParser, Int)
{
	EXPECT_EQ(ParseCommandArgInt("0"), 0);
	EXPECT_EQ(ParseCommandArgInt("42"), 42);
	EXPECT_EQ(ParseCommandArgInt("-42"), -42);
	EXPECT_EQ(ParseCommandArgInt("+42"), 42);
	EXPECT_EQ(ParseCommandArgInt("2147483647"), 2147483647);
	EXPECT_EQ(ParseCommandArgInt("-2147483648"), -2147483647 - 1);

	EXPECT_THROW(ParseCommandArgInt(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("abc"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("1x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("1.5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("+-5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("++5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("+"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt(" 1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("2147483648"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("-2147483649"), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("99999999999999999999"), ProtocolError);
}

TEST(ArgParser, IntMinMax)
{
	EXPECT_EQ(ParseCommandArgInt("-100", -100, 100), -100);
	EXPECT_EQ(ParseCommandArgInt("100", -100, 100), 100);
	EXPECT_EQ(ParseCommandArgInt("+5", -100, 100), 5);

	EXPECT_THROW(ParseCommandArgInt("-101", -100, 100), ProtocolError);
	EXPECT_THROW(ParseCommandArgInt("101", -100, 100), ProtocolError);
}

TEST(ArgParser, Range)
{
	RangeArg range = ParseCommandArgRange("1");
	EXPECT_EQ(1u, range.start);
	EXPECT_EQ(2u, range.end);

	range = ParseCommandArgRange("1:5");
	EXPECT_EQ(1u, range.start);
	EXPECT_EQ(5u, range.end);

	range = ParseCommandArgRange("1:");
	EXPECT_EQ(1u, range.start);
	EXPECT_GE(range.end, 999999u);

	EXPECT_THROW(range = ParseCommandArgRange("-2"),
		     ProtocolError);
}

TEST(ArgParser, RangeEdgeCases)
{
	EXPECT_EQ(ParseCommandArgRange("0"), RangeArg::Single(0));
	EXPECT_EQ(ParseCommandArgRange("0:0"), (RangeArg{0, 0}));
	EXPECT_EQ(ParseCommandArgRange("3:3"), (RangeArg{3, 3}));
	EXPECT_EQ(ParseCommandArgRange("0:"), RangeArg::All());
	EXPECT_EQ(ParseCommandArgRange("2147483647"),
		  RangeArg::Single(2147483647));
	EXPECT_EQ(ParseCommandArgRange("0:2147483647"),
		  (RangeArg{0, 2147483647}));

	/* compatibility with older MPD versions */
	EXPECT_TRUE(ParseCommandArgRange("-1").IsAll());

	EXPECT_THROW(ParseCommandArgRange(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange(":5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange(":"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:5x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:2:3"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("5:3"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("-1:5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:-1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:-5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("+1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("1:+5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange(" 1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("2147483648"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("0:2147483648"), ProtocolError);
	EXPECT_THROW(ParseCommandArgRange("99999999999999999999"), ProtocolError);
}

TEST(ArgParser, Unsigned)
{
	EXPECT_EQ(ParseCommandArgUnsigned("0"), 0U);
	EXPECT_EQ(ParseCommandArgUnsigned("42"), 42U);
	EXPECT_EQ(ParseCommandArgUnsigned("4294967295"), 4294967295U);

	EXPECT_THROW(ParseCommandArgUnsigned(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("abc"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("1x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("1.5"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("-1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("+1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned(" 1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("4294967296"), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("99999999999999999999"), ProtocolError);
}

TEST(ArgParser, UnsignedMax)
{
	EXPECT_EQ(ParseCommandArgUnsigned("0", 100), 0U);
	EXPECT_EQ(ParseCommandArgUnsigned("100", 100), 100U);

	EXPECT_THROW(ParseCommandArgUnsigned("101", 100), ProtocolError);
	EXPECT_THROW(ParseCommandArgUnsigned("-1", 100), ProtocolError);
}

TEST(ArgParser, Bool)
{
	EXPECT_FALSE(ParseCommandArgBool("0"));
	EXPECT_TRUE(ParseCommandArgBool("1"));

	EXPECT_THROW(ParseCommandArgBool(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("2"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("-1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("1x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("true"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("yes"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("+1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgBool("99999999999999999999"), ProtocolError);
}

TEST(ArgParser, Float)
{
	EXPECT_FLOAT_EQ(ParseCommandArgFloat("0"), 0.0f);
	EXPECT_FLOAT_EQ(ParseCommandArgFloat("1.5"), 1.5f);
	EXPECT_FLOAT_EQ(ParseCommandArgFloat("-2.5"), -2.5f);
	EXPECT_FLOAT_EQ(ParseCommandArgFloat("+2.5"), 2.5f);
	EXPECT_FLOAT_EQ(ParseCommandArgFloat("1e3"), 1000.0f);

	EXPECT_THROW(ParseCommandArgFloat(""), ProtocolError);
	EXPECT_THROW(ParseCommandArgFloat("abc"), ProtocolError);
	EXPECT_THROW(ParseCommandArgFloat("1.5x"), ProtocolError);
	EXPECT_THROW(ParseCommandArgFloat("1e39"), ProtocolError);
	EXPECT_THROW(ParseCommandArgFloat("-1e39"), ProtocolError);
}

TEST(ArgParser, SongTime)
{
	EXPECT_EQ(ParseCommandArgSongTime("0").ToMS(), 0U);
	EXPECT_EQ(ParseCommandArgSongTime("1.5").ToMS(), 1500U);
	EXPECT_EQ(ParseCommandArgSongTime("60").ToMS(), 60000U);

	EXPECT_THROW(ParseCommandArgSongTime("-1"), ProtocolError);
	EXPECT_THROW(ParseCommandArgSongTime("x"), ProtocolError);
}

TEST(ArgParser, SignedSongTime)
{
	EXPECT_EQ(ParseCommandArgSignedSongTime("0").ToMS(), 0);
	EXPECT_EQ(ParseCommandArgSignedSongTime("1.5").ToMS(), 1500);
	EXPECT_EQ(ParseCommandArgSignedSongTime("-1.5").ToMS(), -1500);
	EXPECT_EQ(ParseCommandArgSignedSongTime("+2").ToMS(), 2000);

	EXPECT_THROW(ParseCommandArgSignedSongTime("x"), ProtocolError);
}
