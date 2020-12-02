/*
 * Unit tests for src/util/
 */

#include "util/UriExtract.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UriExtract, Suffix)
{
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo/bar").data());
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo.jpg/bar").data());
	EXPECT_EQ(uri_get_suffix("/foo/bar.jpg"), "jpg"sv);
	EXPECT_EQ(uri_get_suffix("/foo.png/bar.jpg"), "jpg"sv);
	EXPECT_EQ((const char *)nullptr, uri_get_suffix(".jpg").data());
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo/.jpg").data());

	/* eliminate the query string */
	EXPECT_EQ(uri_get_suffix("/foo/bar.jpg?query_string"), "jpg"sv);
}
