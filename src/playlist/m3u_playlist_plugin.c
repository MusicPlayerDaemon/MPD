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
#include "playlist/m3u_playlist_plugin.h"
#include "playlist_plugin.h"
#include "text_input_stream.h"
#include "uri.h"
#include "song.h"

#include <glib.h>

struct m3u_playlist {
	struct playlist_provider base;

	struct text_input_stream *tis;
};

static struct playlist_provider *
m3u_open_stream(struct input_stream *is)
{
	struct m3u_playlist *playlist = g_new(struct m3u_playlist, 1);

	playlist_provider_init(&playlist->base, &m3u_playlist_plugin);
	playlist->tis = text_input_stream_new(is);

	return &playlist->base;
}

static void
m3u_close(struct playlist_provider *_playlist)
{
	struct m3u_playlist *playlist = (struct m3u_playlist *)_playlist;

	text_input_stream_free(playlist->tis);
	g_free(playlist);
}

static struct song *
m3u_read(struct playlist_provider *_playlist)
{
	struct m3u_playlist *playlist = (struct m3u_playlist *)_playlist;
	const char *line;

	do {
		line = text_input_stream_read(playlist->tis);
		if (line == NULL)
			return NULL;

		while (*line != 0 && g_ascii_isspace(*line))
			++line;
	} while (line[0] == '#' || *line == 0);

	return song_remote_new(line);
}

static const char *const m3u_suffixes[] = {
	"m3u",
	NULL
};

static const char *const m3u_mime_types[] = {
	"audio/x-mpegurl",
	NULL
};

const struct playlist_plugin m3u_playlist_plugin = {
	.name = "m3u",

	.open_stream = m3u_open_stream,
	.close = m3u_close,
	.read = m3u_read,

	.suffixes = m3u_suffixes,
	.mime_types = m3u_mime_types,
};
