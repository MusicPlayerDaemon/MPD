/*
 * Unit tests for src/util/
 */

#include "util/StringStrip.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(StringStrip, StripLeft)
{
	EXPECT_EQ(StripLeft(""sv), ""sv);
	EXPECT_EQ(StripLeft(" "sv), ""sv);
	EXPECT_EQ(StripLeft("\t"sv), ""sv);
	EXPECT_EQ(StripLeft("\0"sv), ""sv);
	EXPECT_EQ(StripLeft(" a "sv), "a "sv);
	EXPECT_EQ(StripLeft("\0a\0"sv), "a\0"sv);
}

TEST(StringStrip, StripRight)
{
	EXPECT_EQ(StripRight(""sv), ""sv);
	EXPECT_EQ(StripRight(" "sv), ""sv);
	EXPECT_EQ(StripRight("\t"sv), ""sv);
	EXPECT_EQ(StripRight("\0"sv), ""sv);
	EXPECT_EQ(StripRight(" a "sv), " a"sv);
	EXPECT_EQ(StripRight("\0a\0"sv), "\0a"sv);
}

TEST(StringStrip, Strip)
{
	EXPECT_EQ(Strip(""sv), ""sv);
	EXPECT_EQ(Strip(" "sv), ""sv);
	EXPECT_EQ(Strip("\t"sv), ""sv);
	EXPECT_EQ(Strip("\0"sv), ""sv);
	EXPECT_EQ(Strip(" a "sv), "a"sv);
	EXPECT_EQ(Strip("\0a\0"sv), "a"sv);
}
