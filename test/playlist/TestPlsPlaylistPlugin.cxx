// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistUtil.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/SongEnumerator.hxx"
#include "config/Data.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

// from https://en.wikipedia.org/wiki/PLS_(file_format)
static constexpr auto pls1 = R"(
[playlist]
File1=https://e20.yesstreaming.net:8279/
Title1=Here enter name of the station
NumberOfEntries=1
)"sv;

static constexpr auto expect1 = R"(
song_begin: https://e20.yesstreaming.net:8279/
Title: Here enter name of the station
song_end

)"sv;

// from https://en.wikipedia.org/wiki/PLS_(file_format)
static constexpr auto pls2 = R"pls(
[playlist]

File1=https://e20.yesstreaming.net:8279/
Length1=-1

File2=example2.mp3
Title2=Just some local audio that is 2mins long
Length2=120

File3=F:\Music\whatever.m4a
Title3=absolute path on Windows

File4=%UserProfile%\Music\short.ogg
Title4=example for an Environment variable
Length4=5

NumberOfEntries=4
Version=2
)pls"sv;

static constexpr auto expect2 = R"(
song_begin: https://e20.yesstreaming.net:8279/
song_end

song_begin: example2.mp3
Time: 120
Title: Just some local audio that is 2mins long
song_end

song_begin: F:\Music\whatever.m4a
Title: absolute path on Windows
song_end

song_begin: %UserProfile%\Music\short.ogg
Time: 5
Title: example for an Environment variable
song_end

)"sv;

TEST(PlaylistPlugins, Pls)
{
	const ConfigData config;
	ScopePlaylistPluginsInit playlist_plugins_init{config};

	const char *uri = "dummy.pls";

	{
		const auto p = ParsePlaylist(uri, pls1);
		ASSERT_TRUE(p);
		EXPECT_EQ(ToString(*p), expect1);
	}

	{
		const auto p = ParsePlaylist(uri, pls2);
		ASSERT_TRUE(p);
		EXPECT_EQ(ToString(*p), expect2);
	}
}
