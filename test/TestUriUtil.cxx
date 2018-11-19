/*
 * Unit tests for src/util/
 */

#include "util/UriUtil.hxx"

#include <gtest/gtest.h>

TEST(UriUtil, Suffix)
{
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo/bar"));
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo.jpg/bar"));
	EXPECT_STREQ(uri_get_suffix("/foo/bar.jpg"), "jpg");
	EXPECT_STREQ(uri_get_suffix("/foo.png/bar.jpg"), "jpg");
	EXPECT_EQ((const char *)nullptr, uri_get_suffix(".jpg"));
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo/.jpg"));

	/* the first overload does not eliminate the query
	   string */
	EXPECT_STREQ(uri_get_suffix("/foo/bar.jpg?query_string"),
		     "jpg?query_string");

	/* ... but the second one does */
	UriSuffixBuffer buffer;
	EXPECT_STREQ(uri_get_suffix("/foo/bar.jpg?query_string", buffer),
		     "jpg");

	/* repeat some of the above tests with the second overload */
	EXPECT_EQ((const char *)nullptr, uri_get_suffix("/foo/bar", buffer));
	EXPECT_EQ((const char *)nullptr,
		  uri_get_suffix("/foo.jpg/bar", buffer));
	EXPECT_STREQ(uri_get_suffix("/foo/bar.jpg", buffer), "jpg");
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
