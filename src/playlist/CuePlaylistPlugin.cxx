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
#include "song.h"
#include "input_stream.h"

extern "C" {
#include "text_input_stream.h"
#include "cue/cue_parser.h"
}

#include <glib.h>
#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cue"

struct CuePlaylist {
	struct playlist_provider base;

	struct input_stream *is;
	struct text_input_stream *tis;
	struct cue_parser *parser;
};

static struct playlist_provider *
cue_playlist_open_stream(struct input_stream *is)
{
	CuePlaylist *playlist = g_new(CuePlaylist, 1);
	playlist_provider_init(&playlist->base, &cue_playlist_plugin);

	playlist->is = is;
	playlist->tis = text_input_stream_new(is);
	playlist->parser = cue_parser_new();

	return &playlist->base;
}

static void
cue_playlist_close(struct playlist_provider *_playlist)
{
	CuePlaylist *playlist = (CuePlaylist *)_playlist;

	cue_parser_free(playlist->parser);
	text_input_stream_free(playlist->tis);
	g_free(playlist);
}

static struct song *
cue_playlist_read(struct playlist_provider *_playlist)
{
	CuePlaylist *playlist = (CuePlaylist *)_playlist;

	struct song *song = cue_parser_get(playlist->parser);
	if (song != NULL)
		return song;

	const char *line;
	while ((line = text_input_stream_read(playlist->tis)) != NULL) {
		cue_parser_feed(playlist->parser, line);
		song = cue_parser_get(playlist->parser);
		if (song != NULL)
			return song;
	}

	cue_parser_finish(playlist->parser);
	return cue_parser_get(playlist->parser);
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
