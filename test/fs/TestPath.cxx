// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"

#include <gtest/gtest.h>

TEST(Path, Basic)
{
	EXPECT_TRUE(Path{nullptr}.IsNull());
	EXPECT_FALSE(Path::FromFS(PATH_LITERAL("")).IsNull());
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("")).length(), 0U);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("foo")).length(), 3U);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("foo")).ToUTF8(), "foo");
}

TEST(Path, GetBase)
{
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("")).GetBase().c_str(),
		     PATH_LITERAL(""));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("foo")).GetBase().c_str(),
		     PATH_LITERAL("foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("./foo")).GetBase().c_str(),
		     PATH_LITERAL("foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("../foo")).GetBase().c_str(),
		     PATH_LITERAL("foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("foo/bar")).GetBase().c_str(),
		     PATH_LITERAL("bar"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/bar")).GetBase().c_str(),
		     PATH_LITERAL("bar"));
}

TEST(Path, GetDirectoryName)
{
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("")).GetDirectoryName().c_str(),
		     PATH_LITERAL("."));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("foo")).GetDirectoryName().c_str(),
		     PATH_LITERAL("."));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("./foo")).GetDirectoryName().c_str(),
		     PATH_LITERAL("."));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("../foo")).GetDirectoryName().c_str(),
		     PATH_LITERAL(".."));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("foo/bar")).GetDirectoryName().c_str(),
		     PATH_LITERAL("foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/bar")).GetDirectoryName().c_str(),
		     PATH_LITERAL("/foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/")).GetDirectoryName().c_str(),
		     PATH_LITERAL("/foo"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/bar/baz")).GetDirectoryName().c_str(),
		     PATH_LITERAL("/foo/bar"));
}

TEST(Path, Relative)
{
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("foo")).Relative(Path::FromFS(PATH_LITERAL(""))), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/bar")).Relative(Path::FromFS(PATH_LITERAL("/foo/bar"))), nullptr);
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo")).Relative(Path::FromFS(PATH_LITERAL("/foo/bar"))),
		  PATH_LITERAL("bar"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/")).Relative(Path::FromFS(PATH_LITERAL("/foo/bar"))),
		     PATH_LITERAL("bar"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo")).Relative(Path::FromFS(PATH_LITERAL("/foo///bar"))),
		     PATH_LITERAL("bar"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo")).Relative(Path::FromFS(PATH_LITERAL("/foo///"))),
		     PATH_LITERAL(""));
}

TEST(Path, Extension)
{
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("foo")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/bar")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/./bar")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/.bar")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/.")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/..")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar")).GetExtension(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo.abc/")).GetExtension(), nullptr);
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def")).GetExtension(),
		     PATH_LITERAL("def"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.")).GetExtension(),
		     PATH_LITERAL(""));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def.ghi")).GetExtension(),
		     PATH_LITERAL("ghi"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc")).GetExtension(),
		     PATH_LITERAL("abc"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc.def")).GetExtension(),
		     PATH_LITERAL("def"));
}

TEST(Path, Suffix)
{
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("foo")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/bar")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/./bar")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/.bar")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/.")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo/..")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar")).GetSuffix(), nullptr);
	EXPECT_EQ(Path::FromFS(PATH_LITERAL("/foo.abc/")).GetSuffix(), nullptr);
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def")).GetSuffix(),
		     PATH_LITERAL(".def"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.")).GetSuffix(),
		     PATH_LITERAL("."));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def.ghi")).GetSuffix(),
		     PATH_LITERAL(".ghi"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc")).GetSuffix(),
		     PATH_LITERAL(".abc"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc.def")).GetSuffix(),
		     PATH_LITERAL(".def"));
}

TEST(Path, WithSuffix)
{
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("foo")).WithSuffix(PATH_LITERAL(".abc")).c_str(),
		     PATH_LITERAL("foo.abc"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/bar")).WithSuffix(PATH_LITERAL(".abc")).c_str(),
		     PATH_LITERAL("/foo/bar.abc"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.xyz/bar")).WithSuffix(PATH_LITERAL(".abc")).c_str(),
		     PATH_LITERAL("/foo.xyz/bar.abc"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def")).WithSuffix(PATH_LITERAL(".xyz")).c_str(),
		     PATH_LITERAL("/foo.abc/bar.xyz"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo.abc/bar.def.ghi")).WithSuffix(PATH_LITERAL(".xyz")).c_str(),
		     PATH_LITERAL("/foo.abc/bar.def.xyz"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc")).WithSuffix(PATH_LITERAL(".xyz")).c_str(),
		     PATH_LITERAL("/foo/.bar.xyz"));
	EXPECT_STREQ(Path::FromFS(PATH_LITERAL("/foo/.bar.abc.def")).WithSuffix(PATH_LITERAL(".xyz")).c_str(),
		     PATH_LITERAL("/foo/.bar.abc.xyz"));
}
