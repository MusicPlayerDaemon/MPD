/*
 * Unit tests for src/fs/
 */

#include "config.h"
#include "fs/Glob.hxx"

#include <gtest/gtest.h>

#ifdef HAVE_CLASS_GLOB

TEST(Glob, Basic)
{
	const Glob glob("foo");
	EXPECT_TRUE(glob.Check("foo"));
	EXPECT_FALSE(glob.Check("fooo"));
	EXPECT_FALSE(glob.Check("_foo"));
	EXPECT_FALSE(glob.Check("a/foo"));
	EXPECT_FALSE(glob.Check(""));
	EXPECT_FALSE(glob.Check("*"));
}

TEST(Glob, Asterisk)
{
	const Glob glob("*");
	EXPECT_TRUE(glob.Check("foo"));
	EXPECT_TRUE(glob.Check("bar"));
	EXPECT_TRUE(glob.Check("*"));
	EXPECT_TRUE(glob.Check("?"));
}

TEST(Glob, QuestionMark)
{
	const Glob glob("foo?bar");
	EXPECT_TRUE(glob.Check("foo_bar"));
	EXPECT_TRUE(glob.Check("foo?bar"));
	EXPECT_TRUE(glob.Check("foo bar"));
	EXPECT_FALSE(glob.Check("foobar"));
	EXPECT_FALSE(glob.Check("foo__bar"));
}

TEST(Glob, Wildcard)
{
	const Glob glob("foo*bar");
	EXPECT_TRUE(glob.Check("foo_bar"));
	EXPECT_TRUE(glob.Check("foo?bar"));
	EXPECT_TRUE(glob.Check("foo bar"));
	EXPECT_TRUE(glob.Check("foobar"));
	EXPECT_TRUE(glob.Check("foo__bar"));
	EXPECT_FALSE(glob.Check("_foobar"));
	EXPECT_FALSE(glob.Check("foobar_"));
}

TEST(Glob, PrefixWildcard)
{
	const Glob glob("*bar");
	EXPECT_TRUE(glob.Check("foo_bar"));
	EXPECT_TRUE(glob.Check("foo?bar"));
	EXPECT_TRUE(glob.Check("foo bar"));
	EXPECT_TRUE(glob.Check("foobar"));
	EXPECT_TRUE(glob.Check("foo__bar"));
	EXPECT_TRUE(glob.Check("_foobar"));
	EXPECT_TRUE(glob.Check("bar"));
	EXPECT_FALSE(glob.Check("foobar_"));
}

TEST(Glob, SuffixWildcard)
{
	const Glob glob("foo*");
	EXPECT_TRUE(glob.Check("foo_bar"));
	EXPECT_TRUE(glob.Check("foo?bar"));
	EXPECT_TRUE(glob.Check("foo bar"));
	EXPECT_TRUE(glob.Check("foobar"));
	EXPECT_TRUE(glob.Check("foo__bar"));
	EXPECT_TRUE(glob.Check("foobar_"));
	EXPECT_TRUE(glob.Check("foo"));
}

#endif
