// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistUtil.hxx"
#include "playlist/PlaylistRegistry.hxx"
#include "playlist/SongEnumerator.hxx"
#include "config/Data.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

static constexpr auto xspf1 = R"(<?xml version="1.0" encoding="UTF-8"?>
<playlist version="1" xmlns="http://xspf.org/ns/0/">
  <trackList>
    <track>
      <location>http://example.com/a.flac</location>
      <title>A</title>
      <duration>232615</duration>
    </track>
    <track>
      <location>http://example.com/b.flac</location>
      <duration>-1</duration>
    </track>
    <track>
      <location>http://example.com/c.flac</location>
      <duration>3:26</duration>
    </track>
  </trackList>
</playlist>
)"sv;

static constexpr auto expect1 = R"(
song_begin: http://example.com/a.flac
Time: 232.615
Title: A
song_end

song_begin: http://example.com/b.flac
song_end

song_begin: http://example.com/c.flac
song_end

)"sv;

TEST(PlaylistPlugins, Xspf)
{
	const ConfigData config;
	ScopePlaylistPluginsInit playlist_plugins_init{config};

	const auto p = ParsePlaylist("dummy.xspf", xspf1);
	ASSERT_TRUE(p);
	EXPECT_EQ(ToString(*p), expect1);
}
