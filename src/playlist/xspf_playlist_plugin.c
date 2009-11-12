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
#include "playlist/xspf_playlist_plugin.h"
#include "playlist_plugin.h"
#include "input_stream.h"
#include "uri.h"
#include "song.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "xspf"

/**
 * This is the state object for the GLib XML parser.
 */
struct xspf_parser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	GSList *songs;

	/**
	 * The current position in the XML file.
	 */
	enum {
		ROOT, PLAYLIST, TRACKLIST, TRACK,
		LOCATION,
	} state;

	/**
	 * The current tag within the "track" element.  This is only
	 * valid if state==TRACK.  TAG_NUM_OF_ITEM_TYPES means there
	 * is no (known) tag.
	 */
	enum tag_type tag;

	/**
	 * The current song.  It is allocated after the "location"
	 * element.
	 */
	struct song *song;
};

static void
xspf_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
		   const gchar *element_name,
		   G_GNUC_UNUSED const gchar **attribute_names,
		   G_GNUC_UNUSED const gchar **attribute_values,
		   gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct xspf_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		if (strcmp(element_name, "playlist") == 0)
			parser->state = PLAYLIST;

		break;

	case PLAYLIST:
		if (strcmp(element_name, "trackList") == 0)
			parser->state = TRACKLIST;

		break;

	case TRACKLIST:
		if (strcmp(element_name, "track") == 0) {
			parser->state = TRACK;
			parser->song = NULL;
			parser->tag = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case TRACK:
		if (strcmp(element_name, "location") == 0)
			parser->state = LOCATION;
		else if (strcmp(element_name, "title") == 0)
			parser->tag = TAG_TITLE;
		else if (strcmp(element_name, "creator") == 0)
			/* TAG_COMPOSER would be more correct
			   according to the XSPF spec */
			parser->tag = TAG_ARTIST;
		else if (strcmp(element_name, "annotation") == 0)
			parser->tag = TAG_COMMENT;
		else if (strcmp(element_name, "album") == 0)
			parser->tag = TAG_ALBUM;
		else if (strcmp(element_name, "trackNum") == 0)
			parser->tag = TAG_TRACK;

		break;

	case LOCATION:
		break;
	}
}

static void
xspf_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
		 const gchar *element_name,
		 gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct xspf_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		break;

	case PLAYLIST:
		if (strcmp(element_name, "playlist") == 0)
			parser->state = ROOT;

		break;

	case TRACKLIST:
		if (strcmp(element_name, "tracklist") == 0)
			parser->state = PLAYLIST;

		break;

	case TRACK:
		if (strcmp(element_name, "track") == 0) {
			if (parser->song != NULL)
				parser->songs = g_slist_prepend(parser->songs,
								parser->song);

			parser->state = TRACKLIST;
		} else
			parser->tag = TAG_NUM_OF_ITEM_TYPES;

		break;

	case LOCATION:
		parser->state = TRACK;
		break;
	}
}

static void
xspf_text(G_GNUC_UNUSED GMarkupParseContext *context,
	  const gchar *text, gsize text_len,
	  gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct xspf_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
	case PLAYLIST:
	case TRACKLIST:
		break;

	case TRACK:
		if (parser->song != NULL &&
		    parser->tag != TAG_NUM_OF_ITEM_TYPES) {
			if (parser->song->tag == NULL)
				parser->song->tag = tag_new();
			tag_add_item_n(parser->song->tag, parser->tag,
				       text, text_len);
		}

		break;

	case LOCATION:
		if (parser->song == NULL) {
			char *uri = g_strndup(text, text_len);
			parser->song = song_remote_new(uri);
			g_free(uri);
		}

		break;
	}
}

static const GMarkupParser xspf_parser = {
	.start_element = xspf_start_element,
	.end_element = xspf_end_element,
	.text = xspf_text,
};

static void
song_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct song *song = data;

	song_free(song);
}

static void
xspf_parser_destroy(gpointer data)
{
	struct xspf_parser *parser = data;

	if (parser->state >= TRACK && parser->song != NULL)
		song_free(parser->song);

	g_slist_foreach(parser->songs, song_free_callback, NULL);
	g_slist_free(parser->songs);
}

/*
 * The playlist object
 *
 */

struct xspf_playlist {
	struct playlist_provider base;

	GSList *songs;
};

static struct playlist_provider *
xspf_open_stream(struct input_stream *is)
{
	struct xspf_parser parser = {
		.songs = NULL,
		.state = ROOT,
	};
	struct xspf_playlist *playlist;
	GMarkupParseContext *context;
	char buffer[1024];
	size_t nbytes;
	bool success;
	GError *error = NULL;

	/* parse the XSPF XML file */

	context = g_markup_parse_context_new(&xspf_parser,
					     G_MARKUP_TREAT_CDATA_AS_TEXT,
					     &parser, xspf_parser_destroy);

	while (true) {
		nbytes = input_stream_read(is, buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		success = g_markup_parse_context_parse(context, buffer, nbytes,
						       &error);
		if (!success) {
			g_warning("XML parser failed: %s", error->message);
			g_error_free(error);
			g_markup_parse_context_free(context);
			return NULL;
		}
	}

	success = g_markup_parse_context_end_parse(context, &error);
	if (!success) {
		g_warning("XML parser failed: %s", error->message);
		g_error_free(error);
		g_markup_parse_context_free(context);
		return NULL;
	}

	/* create a #xspf_playlist object from the parsed song list */

	playlist = g_new(struct xspf_playlist, 1);
	playlist_provider_init(&playlist->base, &xspf_playlist_plugin);
	playlist->songs = g_slist_reverse(parser.songs);
	parser.songs = NULL;

	g_markup_parse_context_free(context);

	return &playlist->base;
}

static void
xspf_close(struct playlist_provider *_playlist)
{
	struct xspf_playlist *playlist = (struct xspf_playlist *)_playlist;

	g_slist_foreach(playlist->songs, song_free_callback, NULL);
	g_slist_free(playlist->songs);
	g_free(playlist);
}

static struct song *
xspf_read(struct playlist_provider *_playlist)
{
	struct xspf_playlist *playlist = (struct xspf_playlist *)_playlist;
	struct song *song;

	if (playlist->songs == NULL)
		return NULL;

	song = playlist->songs->data;
	playlist->songs = g_slist_remove(playlist->songs, song);

	return song;
}

static const char *const xspf_suffixes[] = {
	"xspf",
	NULL
};

static const char *const xspf_mime_types[] = {
	"application/xspf+xml",
	NULL
};

const struct playlist_plugin xspf_playlist_plugin = {
	.name = "xspf",

	.open_stream = xspf_open_stream,
	.close = xspf_close,
	.read = xspf_read,

	.suffixes = xspf_suffixes,
	.mime_types = xspf_mime_types,
};
