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

TEST(UriExtract, Origin)
{
	EXPECT_EQ(UriGetOrigin("http://example.com/"sv), "http://example.com"sv);
	EXPECT_EQ(UriGetOrigin("http://example.com/foo/bar"sv),
		  "http://example.com"sv);
	EXPECT_EQ(UriGetOrigin("https://example.com:8080/foo"sv),
		  "https://example.com:8080"sv);
	EXPECT_EQ(UriGetOrigin("smb://user:pass@server/share"sv),
		  "smb://user:pass@server"sv);
	EXPECT_EQ(UriGetOrigin("http://[::1]:6600/foo"sv),
		  "http://[::1]:6600"sv);

	/* no path */
	EXPECT_EQ(UriGetOrigin("http://example.com"sv), "http://example.com"sv);

	/* the authority also ends at a query string or fragment */
	EXPECT_EQ(UriGetOrigin("http://example.com?x=1"sv), "http://example.com"sv);
	EXPECT_EQ(UriGetOrigin("http://example.com#frag"sv), "http://example.com"sv);
	EXPECT_EQ(UriGetOrigin("http://example.com:80?x=/y"sv),
		  "http://example.com:80"sv);
	EXPECT_EQ(UriGetOrigin("http://example.com#/x"sv), "http://example.com"sv);
	EXPECT_EQ(UriGetOrigin("http://example.com/?x#y"sv), "http://example.com"sv);

	/* protocol-relative URI */
	EXPECT_EQ(UriGetOrigin("//example.com/foo"sv), "//example.com"sv);

	/* the result points into the given string */
	constexpr std::string_view uri = "http://example.com/foo"sv;
	EXPECT_EQ(UriGetOrigin(uri).data(), uri.data());

	/* views which are not null-terminated */
	EXPECT_EQ(UriGetOrigin(uri.substr(0, 14)), "http://example"sv);

	/* no scheme/authority: null result */
	EXPECT_EQ(UriGetOrigin(""sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("foo"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("foo/bar"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("/foo/bar"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("http:"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("http://"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("http:///foo"sv).data(), nullptr);
	EXPECT_EQ(UriGetOrigin("file:///foo"sv).data(), nullptr);
}
