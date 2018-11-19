/*
 * Unit tests for src/util/
 */

#include "util/MimeType.hxx"

#include <gtest/gtest.h>

TEST(MimeType, Base)
{
	EXPECT_EQ("", GetMimeTypeBase(""));
	EXPECT_EQ("", GetMimeTypeBase(";"));
	EXPECT_EQ("foo", GetMimeTypeBase("foo"));
	EXPECT_EQ("foo/bar", GetMimeTypeBase("foo/bar"));
	EXPECT_EQ("foo/bar", GetMimeTypeBase("foo/bar;"));
	EXPECT_EQ("foo/bar", GetMimeTypeBase("foo/bar; x=y"));
	EXPECT_EQ("foo/bar", GetMimeTypeBase("foo/bar;x=y"));
}

TEST(UriUtil, Parameters)
{
	EXPECT_TRUE(ParseMimeTypeParameters("").empty());
	EXPECT_TRUE(ParseMimeTypeParameters("foo/bar").empty());
	EXPECT_TRUE(ParseMimeTypeParameters("foo/bar;").empty());
	EXPECT_TRUE(ParseMimeTypeParameters("foo/bar;garbage").empty());
	EXPECT_TRUE(ParseMimeTypeParameters("foo/bar; garbage").empty());

	auto p = ParseMimeTypeParameters("foo/bar;a=b");
	EXPECT_FALSE(p.empty());
	EXPECT_EQ(p["a"], "b");
	EXPECT_EQ(p.size(), 1u);

	p = ParseMimeTypeParameters("foo/bar; a=b;c;d=e ; f=g ");
	EXPECT_FALSE(p.empty());
	EXPECT_EQ(p["a"], "b");
	EXPECT_EQ(p["d"], "e");
	EXPECT_EQ(p["f"], "g");
	EXPECT_EQ(p.size(), 3u);
}
