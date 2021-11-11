/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ExtM3uPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Tag.hxx"
#include "tag/Builder.hxx"
#include "util/StringStrip.hxx"
#include "util/StringCompare.hxx"
#include "input/TextInputStream.hxx"
#include "input/InputStream.hxx"

#include <string.h>
#include <stdlib.h>

class ExtM3uPlaylist final : public SongEnumerator {
	TextInputStream tis;

public:
	explicit ExtM3uPlaylist(InputStreamPtr &&is)
		:tis(std::move(is)) {
	}

	/**
	 * @return nullptr if ExtM3U was recognized, or the original
	 * InputStream on error
	 */
	InputStreamPtr CheckFirstLine() {
		char *line = tis.ReadLine();
		if (line == nullptr)
			return tis.StealInputStream();

		StripRight(line);
		if (strcmp(line, "#EXTM3U") != 0)
			return tis.StealInputStream();

		return nullptr;
	}

	std::unique_ptr<DetachedSong> NextSong() override;
};

static std::unique_ptr<SongEnumerator>
extm3u_open_stream(InputStreamPtr &&is)
{
	auto playlist = std::make_unique<ExtM3uPlaylist>(std::move(is));

	is = playlist->CheckFirstLine();
	if (is)
		/* no EXTM3U header: fall back to the plain m3u
		   plugin */
		playlist.reset();

	return playlist;
}

/**
 * Parse a EXTINF line.
 *
 * @param line the rest of the input line after the colon
 */
static Tag
extm3u_parse_tag(const char *line)
{
	long duration;
	char *endptr;
	const char *name;

	duration = strtol(line, &endptr, 10);
	if (endptr[0] != ',')
		/* malformed line */
		return {};

	if (duration < 0)
		/* 0 means unknown duration */
		duration = 0;

	name = StripLeft(endptr + 1);
	if (*name == 0 && duration == 0)
		/* no information available; don't allocate a tag
		   object */
		return {};

	TagBuilder tag;
	tag.SetDuration(SignedSongTime::FromS(unsigned(duration)));

	/* unfortunately, there is no real specification for the
	   EXTM3U format, so we must assume that the string after the
	   comma is opaque, and is just the song name*/
	if (*name != 0)
		tag.AddItem(TAG_NAME, name);

	return tag.Commit();
}

std::unique_ptr<DetachedSong>
ExtM3uPlaylist::NextSong()
{
	Tag tag;
	char *line_s;

	do {
		line_s = tis.ReadLine();
		if (line_s == nullptr)
			return nullptr;

		StripRight(line_s);

		if (StringStartsWith(line_s, "#EXTINF:")) {
			tag = extm3u_parse_tag(line_s + 8);
			continue;
		}

		line_s = StripLeft(line_s);
	} while (line_s[0] == '#' || *line_s == 0);

	return std::make_unique<DetachedSong>(line_s, std::move(tag));
}

static const char *const extm3u_suffixes[] = {
	"m3u",
	"m3u8",
	nullptr
};

static const char *const extm3u_mime_types[] = {
	"audio/x-mpegurl",
	"audio/mpegurl",
	nullptr
};

constexpr PlaylistPlugin extm3u_playlist_plugin =
	PlaylistPlugin("extm3u", extm3u_open_stream)
	.WithSuffixes(extm3u_suffixes)
	.WithMimeTypes(extm3u_mime_types);
