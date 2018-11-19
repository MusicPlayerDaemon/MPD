/*
 * Unit tests for src/util/
 */

#include "util/DivideString.hxx"

#include <gtest/gtest.h>

TEST(DivideString, Basic)
{
	constexpr char input[] = "foo.bar";
	const DivideString ds(input, '.');
	EXPECT_TRUE(ds.IsDefined());
	EXPECT_FALSE(ds.empty());
	EXPECT_EQ(0, strcmp(ds.GetFirst(), "foo"));
	EXPECT_EQ(input + 4, ds.GetSecond());
}

TEST(DivideString, Empty)
{
	constexpr char input[] = ".bar";
	const DivideString ds(input, '.');
	EXPECT_TRUE(ds.IsDefined());
	EXPECT_TRUE(ds.empty());
	EXPECT_EQ(0, strcmp(ds.GetFirst(), ""));
	EXPECT_EQ(input + 1, ds.GetSecond());
}

TEST(DivideString, Fail)
{
	constexpr char input[] = "foo!bar";
	const DivideString ds(input, '.');
	EXPECT_FALSE(ds.IsDefined());
}

TEST(DivideString, Strip)
{
	constexpr char input[] = " foo\t.\nbar\r";
	const DivideString ds(input, '.', true);
	EXPECT_TRUE(ds.IsDefined());
	EXPECT_FALSE(ds.empty());
	EXPECT_EQ(0, strcmp(ds.GetFirst(), "foo"));
	EXPECT_EQ(input + 7, ds.GetSecond());
}
