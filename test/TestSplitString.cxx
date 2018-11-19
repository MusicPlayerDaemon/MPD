/*
 * Unit tests for src/util/
 */

#include "util/SplitString.hxx"
#include "util/Macros.hxx"

#include <gtest/gtest.h>


TEST(SplitString, Basic)
{
	constexpr char input[] = "foo.bar";
	const char *const output[] = { "foo", "bar" };
	size_t i = 0;
	for (auto p : SplitString(input, '.')) {
		EXPECT_LT(i, ARRAY_SIZE(output));
		EXPECT_EQ(p, output[i]);
		++i;
	}

	EXPECT_EQ(ARRAY_SIZE(output), i);
}

TEST(SplitString, Strip)
{
	constexpr char input[] = " foo\t.\r\nbar\r\n2";
	const char *const output[] = { "foo", "bar\r\n2" };
	size_t i = 0;
	for (auto p : SplitString(input, '.')) {
		EXPECT_LT(i, ARRAY_SIZE(output));
		EXPECT_EQ(p, output[i]);
		++i;
	}

	EXPECT_EQ(ARRAY_SIZE(output), i);
}

TEST(SplitString, NoStrip)
{
	constexpr char input[] = " foo\t.\r\nbar\r\n2";
	const char *const output[] = { " foo\t", "\r\nbar\r\n2" };
	size_t i = 0;
	for (auto p : SplitString(input, '.', false)) {
		EXPECT_LT(i, ARRAY_SIZE(output));
		EXPECT_EQ(p, output[i]);
		++i;
	}

	EXPECT_EQ(ARRAY_SIZE(output), i);
}

TEST(SplitString, Empty)
{
	EXPECT_TRUE(SplitString("", '.').empty());
}
