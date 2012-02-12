/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

/** \file
 *
 * Playlist plugin that reads embedded cue sheets from the "CUESHEET"
 * tag of a music file.
 */

#include "config.h"
#include "playlist/embcue_playlist_plugin.h"
#include "playlist_plugin.h"
#include "tag.h"
#include "tag_handler.h"
#include "tag_file.h"
#include "tag_ape.h"
#include "tag_id3.h"
#include "song.h"
#include "cue/cue_parser.h"

#include <glib.h>
#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cue"

struct embcue_playlist {
	struct playlist_provider base;

	/**
	 * This is an override for the CUE's "FILE".  An embedded CUE
	 * sheet must always point to the song file it is contained
	 * in.
	 */
	char *filename;

	/**
	 * The value of the file's "CUESHEET" tag.
	 */
	char *cuesheet;

	/**
	 * The offset of the next line within "cuesheet".
	 */
	char *next;

	struct cue_parser *parser;
};

static void
embcue_tag_pair(const char *name, const char *value, void *ctx)
{
	struct embcue_playlist *playlist = ctx;

	if (playlist->cuesheet == NULL &&
	    g_ascii_strcasecmp(name, "cuesheet") == 0)
		playlist->cuesheet = g_strdup(value);
}

static const struct tag_handler embcue_tag_handler = {
	.pair = embcue_tag_pair,
};

static struct playlist_provider *
embcue_playlist_open_uri(const char *uri,
			 G_GNUC_UNUSED GMutex *mutex,
			 G_GNUC_UNUSED GCond *cond)
{
	if (!g_path_is_absolute(uri))
		/* only local files supported */
		return NULL;

	struct embcue_playlist *playlist = g_new(struct embcue_playlist, 1);
	playlist_provider_init(&playlist->base, &embcue_playlist_plugin);
	playlist->cuesheet = NULL;

	tag_file_scan(uri, &embcue_tag_handler, playlist);
	if (playlist->cuesheet == NULL) {
		tag_ape_scan2(uri, &embcue_tag_handler, playlist);
		if (playlist->cuesheet == NULL)
			tag_id3_scan(uri, &embcue_tag_handler, playlist);
	}

	if (playlist->cuesheet == NULL) {
		/* no "CUESHEET" tag found */
		g_free(playlist);
		return NULL;
	}

	playlist->filename = g_path_get_basename(uri);

	playlist->next = playlist->cuesheet;
	playlist->parser = cue_parser_new();

	return &playlist->base;
}

static void
embcue_playlist_close(struct playlist_provider *_playlist)
{
	struct embcue_playlist *playlist = (struct embcue_playlist *)_playlist;

	cue_parser_free(playlist->parser);
	g_free(playlist->cuesheet);
	g_free(playlist->filename);
	g_free(playlist);
}

static struct song *
embcue_playlist_read(struct playlist_provider *_playlist)
{
	struct embcue_playlist *playlist = (struct embcue_playlist *)_playlist;

	struct song *song = cue_parser_get(playlist->parser);
	if (song != NULL)
		return song;

	while (*playlist->next != 0) {
		const char *line = playlist->next;
		char *eol = strpbrk(playlist->next, "\r\n");
		if (eol != NULL) {
			/* null-terminate the line */
			*eol = 0;
			playlist->next = eol + 1;
		} else
			/* last line; put the "next" pointer to the
			   end of the buffer */
			playlist->next += strlen(line);

		cue_parser_feed(playlist->parser, line);
		song = cue_parser_get(playlist->parser);
		if (song != NULL)
			return song_replace_uri(song, playlist->filename);
	}

	cue_parser_finish(playlist->parser);
	song = cue_parser_get(playlist->parser);
	if (song != NULL)
		song = song_replace_uri(song, playlist->filename);
	return song;
}

static const char *const embcue_playlist_suffixes[] = {
	/* a few codecs that are known to be supported; there are
	   probably many more */
	"flac",
	"mp3", "mp2",
	"mp4", "mp4a", "m4b",
	"ape",
	"wv",
	"ogg", "oga",
	NULL
};

const struct playlist_plugin embcue_playlist_plugin = {
	.name = "cue",

	.open_uri = embcue_playlist_open_uri,
	.close = embcue_playlist_close,
	.read = embcue_playlist_read,

	.suffixes = embcue_playlist_suffixes,
	.mime_types = NULL,
};
