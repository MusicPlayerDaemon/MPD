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
