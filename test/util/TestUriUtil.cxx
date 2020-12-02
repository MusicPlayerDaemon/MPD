/*
 * Unit tests for src/util/
 */

#include "util/UriUtil.hxx"

#include <gtest/gtest.h>

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
