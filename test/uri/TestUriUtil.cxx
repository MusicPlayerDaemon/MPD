/*
 * Unit tests for src/util/
 */

#include "uri/Util.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UriUtil, SafeLocal)
{
	EXPECT_TRUE(uri_safe_local("foo"));
	EXPECT_TRUE(uri_safe_local("foo/bar"));
	EXPECT_TRUE(uri_safe_local("a/b/c/d/e"));

	// must be non-empty
	EXPECT_FALSE(uri_safe_local(""));

	// must not begin or end with a slash
	EXPECT_FALSE(uri_safe_local("/"));
	EXPECT_FALSE(uri_safe_local("/foo"));
	EXPECT_FALSE(uri_safe_local("foo/"));

	// no double slash
	EXPECT_FALSE(uri_safe_local("foo//bar"));
	EXPECT_FALSE(uri_safe_local("foo///bar"));

	// no "." or ".." segments
	EXPECT_FALSE(uri_safe_local("./foo"));
	EXPECT_FALSE(uri_safe_local("../foo"));
	EXPECT_TRUE(uri_safe_local("foo/.bar"));
	EXPECT_FALSE(uri_safe_local("foo/.."));
	EXPECT_FALSE(uri_safe_local("foo/."));
	EXPECT_FALSE(uri_safe_local("foo/../bar"));
	EXPECT_FALSE(uri_safe_local("foo/./bar"));
	EXPECT_TRUE(uri_safe_local(".foo"));
	EXPECT_TRUE(uri_safe_local("..foo"));
	EXPECT_TRUE(uri_safe_local("...foo"));
	EXPECT_TRUE(uri_safe_local(".....foo"));

#ifdef _WIN32
	// the backslash is a path separator on Windows
	EXPECT_FALSE(uri_safe_local("foo\\bar"));
	EXPECT_FALSE(uri_safe_local("..\\foo"));
	EXPECT_FALSE(uri_safe_local("foo\\..\\..\\bar"));
#else
	EXPECT_TRUE(uri_safe_local("foo\\bar"));
	EXPECT_TRUE(uri_safe_local("..\\foo"));
#endif
}

TEST(UriUtil, RemoveAuth)
{
	EXPECT_EQ(uri_remove_auth("http://www.example.com/"),
		  ""sv);
	EXPECT_EQ(uri_remove_auth("http://foo:bar@www.example.com/"),
		  "http://www.example.com/"sv);
	EXPECT_EQ(uri_remove_auth("http://foo@www.example.com/"),
		  "http://www.example.com/"sv);
	EXPECT_EQ(uri_remove_auth("http://www.example.com/f:oo@bar"),
		  ""sv);
	EXPECT_EQ(uri_remove_auth("ftp://foo:bar@ftp.example.com/"),
		  "ftp://ftp.example.com/"sv);
}

TEST(UriUtil, SquashDotSegments)
{
	EXPECT_EQ(uri_squash_dot_segments(""), ""sv);
	EXPECT_EQ(uri_squash_dot_segments("foo"), "foo"sv);
	EXPECT_EQ(uri_squash_dot_segments("foo/bar"), "foo/bar"sv);
	EXPECT_EQ(uri_squash_dot_segments("./foo/bar"), "foo/bar"sv);
	EXPECT_EQ(uri_squash_dot_segments("foo/./bar"), "foo/bar"sv);
	EXPECT_EQ(uri_squash_dot_segments("foo/bar/."), "foo/bar"sv);
	EXPECT_EQ(uri_squash_dot_segments("foo/bar/.."), "foo"sv);
	EXPECT_EQ(uri_squash_dot_segments("foo/../bar"), "bar"sv);
	EXPECT_EQ(uri_squash_dot_segments("../foo/bar"), "foo/bar"sv);
}
