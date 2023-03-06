// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
