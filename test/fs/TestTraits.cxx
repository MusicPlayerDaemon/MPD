// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "fs/Traits.hxx"

#include <gtest/gtest.h>

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
