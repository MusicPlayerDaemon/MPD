// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config/Path.hxx"
#include "config/Data.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/glue/StandardDirectory.hxx"
#include "fs/XDG.hxx" // for USE_XDG

#include <gtest/gtest.h>

#include <stdexcept>

#ifndef _WIN32

AllocatedPath
GetHomeDir(const char *user_name) noexcept
{
	return AllocatedPath::FromFS(PATH_LITERAL("/home")) / AllocatedPath::FromUTF8(user_name);
}

AllocatedPath
GetHomeDir() noexcept
{
	return GetHomeDir("foo");
}

#endif

AllocatedPath
GetUserConfigDir() noexcept
{
#ifdef _WIN32
	return AllocatedPath::FromFS(PATH_LITERAL("c:\\users\\foo\\config"));
#else
	return GetHomeDir() / AllocatedPath::FromFS(PATH_LITERAL(".config"));
#endif
}

AllocatedPath
GetUserMusicDir() noexcept
{
#ifdef _WIN32
	return AllocatedPath::FromFS(PATH_LITERAL("c:\\users\\foo\\Music"));
#else
	return GetHomeDir() / AllocatedPath::FromFS(PATH_LITERAL("Music"));
#endif
}

AllocatedPath
GetUserCacheDir() noexcept
{
#ifdef _WIN32
	return nullptr;
#else
	return GetHomeDir() / AllocatedPath::FromFS(PATH_LITERAL(".cache"));
#endif
}

AllocatedPath
GetAppCacheDir() noexcept
{
#ifdef _WIN32
	return nullptr;
#else
	return GetUserCacheDir() / AllocatedPath::FromFS(PATH_LITERAL("mpd"));
#endif
}

AllocatedPath
GetUserRuntimeDir() noexcept
{
#ifdef _WIN32
	return nullptr;
#else
	return AllocatedPath::FromFS(PATH_LITERAL("/run/user/foo"));
#endif
}

AllocatedPath
GetAppRuntimeDir() noexcept
{
#ifdef _WIN32
	return nullptr;
#else
	return GetUserRuntimeDir() / AllocatedPath::FromFS(PATH_LITERAL("mpd"));
#endif
}

const char *
ConfigData::GetString([[maybe_unused]] ConfigOption option,
		      const char *default_value) const noexcept
{
	return default_value;
}

TEST(ParsePath, Basic)
{
	EXPECT_THROW(ParsePath(""), std::runtime_error);
	EXPECT_EQ(ParsePath("/"), AllocatedPath::FromFS(PATH_LITERAL("/")));
	EXPECT_EQ(ParsePath("/abc"), AllocatedPath::FromFS(PATH_LITERAL("/abc")));

#ifdef _WIN32
	EXPECT_EQ(ParsePath("c:/abc"), AllocatedPath::FromFS(PATH_LITERAL("c:/abc")));
	EXPECT_EQ(ParsePath("c:\\abc"), AllocatedPath::FromFS(PATH_LITERAL("c:\\abc")));
#endif
}

#ifndef _WIN32

TEST(ParsePath, Tilde)
{
	EXPECT_EQ(ParsePath("~"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo")));
	EXPECT_EQ(ParsePath("~/"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo")));
	EXPECT_EQ(ParsePath("~/abc"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/abc")));
	EXPECT_EQ(ParsePath("~bar"), AllocatedPath::FromFS(PATH_LITERAL("/home/bar")));
	EXPECT_EQ(ParsePath("~bar/"), AllocatedPath::FromFS(PATH_LITERAL("/home/bar")));
	EXPECT_EQ(ParsePath("~bar/abc"), AllocatedPath::FromFS(PATH_LITERAL("/home/bar/abc")));
}

TEST(ParsePath, Home)
{
	EXPECT_EQ(ParsePath("$HOME"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo")));
	EXPECT_EQ(ParsePath("$HOME/"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo")));
	EXPECT_EQ(ParsePath("$HOME/abc"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/abc")));
}

#endif

#ifdef USE_XDG

TEST(ParsePath, XDG)
{
	EXPECT_EQ(ParsePath("$XDG_CONFIG_HOME"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/.config")));
	EXPECT_EQ(ParsePath("$XDG_CONFIG_HOME/abc"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/.config/abc")));
	EXPECT_EQ(ParsePath("$XDG_MUSIC_DIR"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/Music")));
	EXPECT_EQ(ParsePath("$XDG_CACHE_HOME"), AllocatedPath::FromFS(PATH_LITERAL("/home/foo/.cache")));
	EXPECT_EQ(ParsePath("$XDG_RUNTIME_DIR/mpd"), AllocatedPath::FromFS(PATH_LITERAL("/run/user/foo/mpd")));
}

#endif
