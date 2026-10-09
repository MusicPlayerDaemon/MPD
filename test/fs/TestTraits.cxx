// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "fs/Traits.hxx"

#include <gtest/gtest.h>

TEST(PathTraitsUTF8, Relative)
{
	/* equal paths: empty result */
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo"), "");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo/"), "");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo///"), "");

	/* the separator is not included in the result */
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo/bar"), "bar");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo///bar"), "bar");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo/bar/baz"), "bar/baz");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo", "/foo/bar/"), "bar/");

	/* the base may end with a separator */
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo/", "/foo/bar"), "bar");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/foo/", "/foo/"), "");

	/* the root directory */
	EXPECT_STREQ(PathTraitsUTF8::Relative("/", "/foo"), "foo");
	EXPECT_STREQ(PathTraitsUTF8::Relative("/", "/"), "");

	/* URIs (used by the WebDAV storage plugin) */
	EXPECT_STREQ(PathTraitsUTF8::Relative("http://example.com/dav",
					      "http://example.com/dav/foo/bar"),
		     "foo/bar");

	/* mismatch: a prefix which ends in the middle of a name */
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", "/foobar"), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", "/foo.bar"), nullptr);

	/* mismatch: different paths */
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", "/bar"), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", "/fo"), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", ""), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo/bar", "/foo"), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo/", "/foo"), nullptr);

	/* the comparison is case sensitive */
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo", "/FOO/bar"), nullptr);
}

TEST(PathTraitsUTF8, RelativeStringView)
{
	using std::string_view_literals::operator""sv;

	/* equal paths: empty (but not nullptr) result */
	auto result = PathTraitsUTF8::Relative("/foo"sv, "/foo"sv);
	EXPECT_NE(result.data(), nullptr);
	EXPECT_EQ(result, ""sv);

	result = PathTraitsUTF8::Relative("/foo"sv, "/foo///"sv);
	EXPECT_NE(result.data(), nullptr);
	EXPECT_EQ(result, ""sv);

	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/foo/bar"sv), "bar"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/foo///bar"sv), "bar"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/foo/bar/baz"sv), "bar/baz"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo/"sv, "/foo/bar"sv), "bar"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/"sv, "/foo"sv), "foo"sv);

	/* mismatch: nullptr result */
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/foobar"sv).data(), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/bar"sv).data(), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, "/fo"sv).data(), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, ""sv).data(), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo/"sv, "/foo"sv).data(), nullptr);

	/* views which are not null-terminated: only the characters
	   inside the view count */
	constexpr std::string_view path = "/foo/bar/baz"sv;
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, path.substr(0, 8)), "bar"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, path.substr(0, 4)), ""sv);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo"sv, path.substr(0, 3)).data(), nullptr);
	EXPECT_EQ(PathTraitsUTF8::Relative("/foo/bar"sv, path.substr(0, 5)).data(), nullptr);

	/* the base view may be a part of a longer string, too */
	EXPECT_EQ(PathTraitsUTF8::Relative(path.substr(0, 4), "/foo/x"sv), "x"sv);
	EXPECT_EQ(PathTraitsUTF8::Relative(path.substr(0, 4), "/foo/bar"sv), "bar"sv);
}

TEST(PathTraitsUTF8, IsValidFilename)
{
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo"));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo.mp3"));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(" "));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("\xc3\xa4"));

	/* names beginning with dots are only special if they consist
	   only of one or two dots */
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(".foo"));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("..foo"));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("..."));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo."));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo.."));

	/* empty */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(""));

	/* special names */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("."));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(".."));

	/* separators */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("/"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("/foo"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo/"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo/bar"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("./"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("../foo"));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo/.."));
}

TEST(PathTraitsUTF8, IsValidFilenameStringView)
{
	using std::string_view_literals::operator""sv;

	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo"sv));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("foo.mp3"sv));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(".foo"sv));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("..foo"sv));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename("..."sv));

	/* empty */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(""sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(std::string_view{}));

	/* special names */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("."sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(".."sv));

	/* separators */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("/"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("/foo"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo/"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo/bar"sv));

	/* embedded null bytes (which the "const char *" overload
	   cannot see) */
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("\0"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo\0"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("foo\0bar"sv));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename("\0foo"sv));

	/* views which are not null-terminated: only the characters
	   inside the view count */
	constexpr std::string_view foo_bar = "foo/bar"sv;
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(foo_bar.substr(0, 3)));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(foo_bar.substr(4)));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(foo_bar.substr(0, 4)));

	constexpr std::string_view dotdot_foo = "..foo"sv;
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(dotdot_foo.substr(0, 1)));
	EXPECT_FALSE(PathTraitsUTF8::IsValidFilename(dotdot_foo.substr(0, 2)));
	EXPECT_TRUE(PathTraitsUTF8::IsValidFilename(dotdot_foo.substr(0, 3)));
}
