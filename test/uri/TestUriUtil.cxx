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
}

TEST(UriUtil, RemoveAuth)
{
	EXPECT_EQ(std::string(),
		  uri_remove_auth("http://www.example.com/"));
	EXPECT_EQ(std::string("http://www.example.com/"),
		  uri_remove_auth("http://foo:bar@www.example.com/"));
	EXPECT_EQ(std::string("http://www.example.com/"),
		  uri_remove_auth("http://foo@www.example.com/"));
	EXPECT_EQ(std::string(),
		  uri_remove_auth("http://www.example.com/f:oo@bar"));
	EXPECT_EQ(std::string("ftp://ftp.example.com/"),
		  uri_remove_auth("ftp://foo:bar@ftp.example.com/"));
}
