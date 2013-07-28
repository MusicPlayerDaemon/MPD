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
#include "tag.h"
#include "Song.hxx"
#include "input_stream.h"
#include "cue/CueParser.hxx"
#include "TextInputStream.hxx"

#include <glib.h>
#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cue"

struct CuePlaylist {
	struct playlist_provider base;

	struct input_stream *is;
	TextInputStream tis;
	CueParser parser;

	CuePlaylist(struct input_stream *_is)
		:is(_is), tis(is) {
		playlist_provider_init(&base, &cue_playlist_plugin);
	}

	~CuePlaylist() {
	}
};

static struct playlist_provider *
cue_playlist_open_stream(struct input_stream *is)
{
	CuePlaylist *playlist = new CuePlaylist(is);
	return &playlist->base;
}

static void
cue_playlist_close(struct playlist_provider *_playlist)
{
	CuePlaylist *playlist = (CuePlaylist *)_playlist;
	delete playlist;
}

static Song *
cue_playlist_read(struct playlist_provider *_playlist)
{
	CuePlaylist *playlist = (CuePlaylist *)_playlist;

	Song *song = playlist->parser.Get();
	if (song != NULL)
		return song;

	std::string line;
	while (playlist->tis.ReadLine(line)) {
		playlist->parser.Feed(line.c_str());
		song = playlist->parser.Get();
		if (song != NULL)
			return song;
	}

	playlist->parser.Finish();
	return playlist->parser.Get();
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
	cue_playlist_close,
	cue_playlist_read,

	nullptr,
	cue_playlist_suffixes,
	cue_playlist_mime_types,
};
