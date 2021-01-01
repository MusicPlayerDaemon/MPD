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

#include "M3uPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "input/TextInputStream.hxx"
#include "util/StringStrip.hxx"

class M3uPlaylist final : public SongEnumerator {
	TextInputStream tis;

public:
	explicit M3uPlaylist(InputStreamPtr &&is)
		:tis(std::move(is)) {
	}

	std::unique_ptr<DetachedSong> NextSong() override;
};

static std::unique_ptr<SongEnumerator>
m3u_open_stream(InputStreamPtr &&is)
{
	return std::make_unique<M3uPlaylist>(std::move(is));
}

std::unique_ptr<DetachedSong>
M3uPlaylist::NextSong()
{
	char *line_s;

	do {
		line_s = tis.ReadLine();
		if (line_s == nullptr)
			return nullptr;

		line_s = Strip(line_s);
	} while (line_s[0] == '#' || *line_s == 0);

	return std::make_unique<DetachedSong>(line_s);
}

static const char *const m3u_suffixes[] = {
	"m3u",
	"m3u8",
	nullptr
};

static const char *const m3u_mime_types[] = {
	"audio/x-mpegurl",
	"audio/mpegurl",
	nullptr
};

const PlaylistPlugin m3u_playlist_plugin =
	PlaylistPlugin("m3u", m3u_open_stream)
	.WithSuffixes(m3u_suffixes)
	.WithMimeTypes(m3u_mime_types);
