/*
 * Unit tests for src/util/
 */

#include "uri/Extract.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UriExtract, Suffix)
{
	EXPECT_EQ((const char *)nullptr, UriGetSuffix("/foo/bar").data());
	EXPECT_EQ((const char *)nullptr, UriGetSuffix("/foo.jpg/bar").data());
	EXPECT_EQ(UriGetSuffix("/foo/bar.jpg"), "jpg"sv);
	EXPECT_EQ(UriGetSuffix("/foo.png/bar.jpg"), "jpg"sv);
	EXPECT_EQ((const char *)nullptr, UriGetSuffix(".jpg").data());
	EXPECT_EQ((const char *)nullptr, UriGetSuffix("/foo/.jpg").data());

	/* eliminate the query string */
	EXPECT_EQ(UriGetSuffix("/foo/bar.jpg?query_string"), "jpg"sv);
}
