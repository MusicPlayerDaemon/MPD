/*
 * Unit tests for src/util/
 */

#include "uri/Util.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UriUtil, SafeLocal)
{
	EXPECT_TRUE(UriIsCanonicalRelativePath("foo"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("foo/bar"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("a/b/c/d/e"));

	// must be non-empty
	EXPECT_FALSE(UriIsCanonicalRelativePath(""));

	// must not begin or end with a slash
	EXPECT_FALSE(UriIsCanonicalRelativePath("/"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("/foo"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo/"));

	// no double slash
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo//bar"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo///bar"));

	// no "." or ".." segments
	EXPECT_FALSE(UriIsCanonicalRelativePath("./foo"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("../foo"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("foo/.bar"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo/.."));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo/."));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo/../bar"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo/./bar"));
	EXPECT_TRUE(UriIsCanonicalRelativePath(".foo"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("..foo"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("...foo"));
	EXPECT_TRUE(UriIsCanonicalRelativePath(".....foo"));

#ifdef _WIN32
	// the backslash is a path separator on Windows
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo\\bar"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("..\\foo"));
	EXPECT_FALSE(UriIsCanonicalRelativePath("foo\\..\\..\\bar"));
#else
	EXPECT_TRUE(UriIsCanonicalRelativePath("foo\\bar"));
	EXPECT_TRUE(UriIsCanonicalRelativePath("..\\foo"));
#endif
}

TEST(UriUtil, RelativePathWithinDepth)
{
	// a safe local URI is a safe relative URI
	EXPECT_TRUE(UriIsRelativePathWithinDepth("foo", 0));
	EXPECT_TRUE(UriIsRelativePathWithinDepth("foo/bar", 0));

	// leading "./" segments are skipped
	EXPECT_TRUE(UriIsRelativePathWithinDepth("./foo", 0));
	EXPECT_TRUE(UriIsRelativePathWithinDepth("././foo", 0));

	// leading "../" segments are limited by max_up
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../foo", 0));
	EXPECT_TRUE(UriIsRelativePathWithinDepth("../foo", 1));
	EXPECT_TRUE(UriIsRelativePathWithinDepth("../foo/bar", 1));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../../foo", 1));
	EXPECT_TRUE(UriIsRelativePathWithinDepth("../../foo", 2));
	EXPECT_TRUE(UriIsRelativePathWithinDepth(".././../foo", 2));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../.././../foo", 2));

	// no "." or ".." segments after the leading run
	EXPECT_FALSE(UriIsRelativePathWithinDepth("foo/../bar", 1));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../foo/../bar", 2));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("foo/./bar", 0));

	// the rest must be a safe local URI
	EXPECT_FALSE(UriIsRelativePathWithinDepth("", 0));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../", 1));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("..", 1));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("/foo", 0));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("..//foo", 1));
	EXPECT_FALSE(UriIsRelativePathWithinDepth("../foo/", 1));
}

TEST(UriUtil, RemoveAuth)
{
	EXPECT_EQ(std::string(),
		  UriRemoveAuth("http://www.example.com/"));
	EXPECT_EQ(std::string("http://www.example.com/"),
		  UriRemoveAuth("http://foo:bar@www.example.com/"));
	EXPECT_EQ(std::string("http://www.example.com/"),
		  UriRemoveAuth("http://foo@www.example.com/"));
	EXPECT_EQ(std::string(),
		  UriRemoveAuth("http://www.example.com/f:oo@bar"));
	EXPECT_EQ(std::string("ftp://ftp.example.com/"),
		  UriRemoveAuth("ftp://foo:bar@ftp.example.com/"));
}

TEST(UriUtil, SquashDotSegments)
{
	EXPECT_EQ(UriSquashDotSegments(""), ""sv);
	EXPECT_EQ(UriSquashDotSegments("foo"), "foo"sv);
	EXPECT_EQ(UriSquashDotSegments("foo/bar"), "foo/bar"sv);
	EXPECT_EQ(UriSquashDotSegments("./foo/bar"), "foo/bar"sv);
	EXPECT_EQ(UriSquashDotSegments("foo/./bar"), "foo/bar"sv);
	EXPECT_EQ(UriSquashDotSegments("foo/bar/."), "foo/bar"sv);
	EXPECT_EQ(UriSquashDotSegments("foo/bar/.."), "foo"sv);
	EXPECT_EQ(UriSquashDotSegments("foo/../bar"), "bar"sv);
	EXPECT_EQ(UriSquashDotSegments("../foo/bar"), "foo/bar"sv);
}
