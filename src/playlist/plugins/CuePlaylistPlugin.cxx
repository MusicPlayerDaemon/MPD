// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "CuePlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../SongEnumerator.hxx"
#include "../cue/CueParser.hxx"
#include "input/TextInputStream.hxx"

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
