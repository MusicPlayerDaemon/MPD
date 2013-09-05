/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "CuePlaylistPlugin.hxx"
#include "PlaylistPlugin.hxx"
#include "SongEnumerator.hxx"
#include "Tag.hxx"
#include "Song.hxx"
#include "cue/CueParser.hxx"
#include "TextInputStream.hxx"

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cue"

class CuePlaylist final : public SongEnumerator {
	struct input_stream *is;
	TextInputStream tis;
	CueParser parser;

 public:
	CuePlaylist(struct input_stream *_is)
		:is(_is), tis(is) {
	}

	virtual Song *NextSong() override;
};

static SongEnumerator *
cue_playlist_open_stream(struct input_stream *is)
{
	return new CuePlaylist(is);
}

Song *
CuePlaylist::NextSong()
{
	Song *song = parser.Get();
	if (song != NULL)
		return song;

	std::string line;
	while (tis.ReadLine(line)) {
		parser.Feed(line.c_str());
		song = parser.Get();
		if (song != NULL)
			return song;
	}

	parser.Finish();
	return parser.Get();
}

static const char *const cue_playlist_suffixes[] = {
	"cue",
	NULL
};

static const char *const cue_playlist_mime_types[] = {
	"application/x-cue",
	NULL
};

const struct playlist_plugin cue_playlist_plugin = {
	"cue",

	nullptr,
	nullptr,
	nullptr,
	cue_playlist_open_stream,

	nullptr,
	cue_playlist_suffixes,
	cue_playlist_mime_types,
};
