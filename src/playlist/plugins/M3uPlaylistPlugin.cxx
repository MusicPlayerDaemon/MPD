/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "M3uPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "DetachedSong.hxx"
#include "util/StringUtil.hxx"
#include "input/TextInputStream.hxx"

class M3uPlaylist final : public SongEnumerator {
	TextInputStream tis;

public:
	M3uPlaylist(InputStream &is)
		:tis(is) {
	}

	virtual DetachedSong *NextSong() override;
};

static SongEnumerator *
m3u_open_stream(InputStream &is)
{
	return new M3uPlaylist(is);
}

DetachedSong *
M3uPlaylist::NextSong()
{
	char *line_s;

	do {
		line_s = tis.ReadLine();
		if (line_s == nullptr)
			return nullptr;

		line_s = Strip(line_s);
	} while (line_s[0] == '#' || *line_s == 0);

	return new DetachedSong(line_s);
}

static const char *const m3u_suffixes[] = {
	"m3u",
	"m3u8",
	nullptr
};

static const char *const m3u_mime_types[] = {
	"audio/x-mpegurl",
	nullptr
};

const struct playlist_plugin m3u_playlist_plugin = {
	"m3u",

	nullptr,
	nullptr,
	nullptr,
	m3u_open_stream,

	nullptr,
	m3u_suffixes,
	m3u_mime_types,
};
