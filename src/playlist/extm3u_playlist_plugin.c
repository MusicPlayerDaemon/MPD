/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "playlist/extm3u_playlist_plugin.h"
#include "playlist_plugin.h"
#include "text_input_stream.h"
#include "uri.h"
#include "song.h"
#include "tag.h"

#include <glib.h>

#include <string.h>
#include <stdlib.h>

struct extm3u_playlist {
	struct playlist_provider base;

	struct text_input_stream *tis;
};

static struct playlist_provider *
extm3u_open_stream(struct input_stream *is)
{
	struct extm3u_playlist *playlist;
	const char *line;

	playlist = g_new(struct extm3u_playlist, 1);
	playlist->tis = text_input_stream_new(is);

	line = text_input_stream_read(playlist->tis);
	if (line == NULL || strcmp(line, "#EXTM3U") != 0) {
		/* no EXTM3U header: fall back to the plain m3u
		   plugin */
		text_input_stream_free(playlist->tis);
		g_free(playlist);
		return NULL;
	}

	playlist_provider_init(&playlist->base, &extm3u_playlist_plugin);
	return &playlist->base;
}

static void
extm3u_close(struct playlist_provider *_playlist)
{
	struct extm3u_playlist *playlist = (struct extm3u_playlist *)_playlist;

	text_input_stream_free(playlist->tis);
	g_free(playlist);
}

/**
 * Parse a EXTINF line.
 *
 * @param line the rest of the input line after the colon
 */
static struct tag *
extm3u_parse_tag(const char *line)
{
	long duration;
	char *endptr;
	const char *name;
	struct tag *tag;

	duration = strtol(line, &endptr, 10);
	if (endptr[0] != ',')
		/* malformed line */
		return NULL;

	if (duration < 0)
		/* 0 means unknown duration */
		duration = 0;

	name = g_strchug(endptr + 1);
	if (*name == 0 && duration == 0)
		/* no information available; don't allocate a tag
		   object */
		return NULL;

	tag = tag_new();
	tag->time = duration;

	/* unfortunately, there is no real specification for the
	   EXTM3U format, so we must assume that the string after the
	   comma is opaque, and is just the song name*/
	if (*name != 0)
		tag_add_item(tag, TAG_NAME, name);

	return tag;
}

static struct song *
extm3u_read(struct playlist_provider *_playlist)
{
	struct extm3u_playlist *playlist = (struct extm3u_playlist *)_playlist;
	struct tag *tag = NULL;
	const char *line;
	struct song *song;

	do {
		line = text_input_stream_read(playlist->tis);
		if (line == NULL) {
			if (tag != NULL)
				tag_free(tag);
			return NULL;
		}

		if (g_str_has_prefix(line, "#EXTINF:")) {
			if (tag != NULL)
				tag_free(tag);
			tag = extm3u_parse_tag(line + 8);
			continue;
		}

		while (*line != 0 && g_ascii_isspace(*line))
			++line;
	} while (line[0] == '#' || *line == 0);

	song = song_remote_new(line);
	song->tag = tag;
	return song;
}

static const char *const extm3u_suffixes[] = {
	"m3u",
	NULL
};

static const char *const extm3u_mime_types[] = {
	"audio/x-mpegurl",
	NULL
};

const struct playlist_plugin extm3u_playlist_plugin = {
	.name = "extm3u",

	.open_stream = extm3u_open_stream,
	.close = extm3u_close,
	.read = extm3u_read,

	.suffixes = extm3u_suffixes,
	.mime_types = extm3u_mime_types,
};
