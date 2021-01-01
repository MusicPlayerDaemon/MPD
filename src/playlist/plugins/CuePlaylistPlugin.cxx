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

#include "CuePlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "../cue/CueParser.hxx"
#include "input/TextInputStream.hxx"
#include "util/StringView.hxx"

class CuePlaylist final : public SongEnumerator {
	TextInputStream tis;
	CueParser parser;

 public:
	explicit CuePlaylist(InputStreamPtr &&is)
		:tis(std::move(is)) {
	}

	std::unique_ptr<DetachedSong> NextSong() override;
};

static std::unique_ptr<SongEnumerator>
cue_playlist_open_stream(InputStreamPtr &&is)
{
	return std::make_unique<CuePlaylist>(std::move(is));
}

std::unique_ptr<DetachedSong>
CuePlaylist::NextSong()
{
	auto song = parser.Get();
	if (song != nullptr)
		return song;

	const char *line;
	while ((line = tis.ReadLine()) != nullptr) {
		parser.Feed(line);
		song = parser.Get();
		if (song != nullptr)
			return song;
	}

	parser.Finish();
	return parser.Get();
}

static const char *const cue_playlist_suffixes[] = {
	"cue",
	nullptr
};

static const char *const cue_playlist_mime_types[] = {
	"application/x-cue",
	nullptr
};

const PlaylistPlugin cue_playlist_plugin =
	PlaylistPlugin("cue", cue_playlist_open_stream)
	.WithAsFolder()
	.WithSuffixes(cue_playlist_suffixes)
	.WithMimeTypes(cue_playlist_mime_types);
