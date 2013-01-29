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
#include "XspfPlaylistPlugin.hxx"
#include "MemoryPlaylistProvider.hxx"
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
struct XspfParser {
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

	XspfParser()
		:songs(nullptr), state(ROOT) {}
};

static void
xspf_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
		   const gchar *element_name,
		   G_GNUC_UNUSED const gchar **attribute_names,
		   G_GNUC_UNUSED const gchar **attribute_values,
		   gpointer user_data, G_GNUC_UNUSED GError **error)
{
	XspfParser *parser = (XspfParser *)user_data;

	switch (parser->state) {
	case XspfParser::ROOT:
		if (strcmp(element_name, "playlist") == 0)
			parser->state = XspfParser::PLAYLIST;

		break;

	case XspfParser::PLAYLIST:
		if (strcmp(element_name, "trackList") == 0)
			parser->state = XspfParser::TRACKLIST;

		break;

	case XspfParser::TRACKLIST:
		if (strcmp(element_name, "track") == 0) {
			parser->state = XspfParser::TRACK;
			parser->song = NULL;
			parser->tag = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case XspfParser::TRACK:
		if (strcmp(element_name, "location") == 0)
			parser->state = XspfParser::LOCATION;
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

	case XspfParser::LOCATION:
		break;
	}
}

static void
xspf_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
		 const gchar *element_name,
		 gpointer user_data, G_GNUC_UNUSED GError **error)
{
	XspfParser *parser = (XspfParser *)user_data;

	switch (parser->state) {
	case XspfParser::ROOT:
		break;

	case XspfParser::PLAYLIST:
		if (strcmp(element_name, "playlist") == 0)
			parser->state = XspfParser::ROOT;

		break;

	case XspfParser::TRACKLIST:
		if (strcmp(element_name, "tracklist") == 0)
			parser->state = XspfParser::PLAYLIST;

		break;

	case XspfParser::TRACK:
		if (strcmp(element_name, "track") == 0) {
			if (parser->song != NULL)
				parser->songs = g_slist_prepend(parser->songs,
								parser->song);

			parser->state = XspfParser::TRACKLIST;
		} else
			parser->tag = TAG_NUM_OF_ITEM_TYPES;

		break;

	case XspfParser::LOCATION:
		parser->state = XspfParser::TRACK;
		break;
	}
}

static void
xspf_text(G_GNUC_UNUSED GMarkupParseContext *context,
	  const gchar *text, gsize text_len,
	  gpointer user_data, G_GNUC_UNUSED GError **error)
{
	XspfParser *parser = (XspfParser *)user_data;

	switch (parser->state) {
	case XspfParser::ROOT:
	case XspfParser::PLAYLIST:
	case XspfParser::TRACKLIST:
		break;

	case XspfParser::TRACK:
		if (parser->song != NULL &&
		    parser->tag != TAG_NUM_OF_ITEM_TYPES) {
			if (parser->song->tag == NULL)
				parser->song->tag = tag_new();
			tag_add_item_n(parser->song->tag, parser->tag,
				       text, text_len);
		}

		break;

	case XspfParser::LOCATION:
		if (parser->song == NULL) {
			char *uri = g_strndup(text, text_len);
			parser->song = song_remote_new(uri);
			g_free(uri);
		}

		break;
	}
}

static const GMarkupParser xspf_parser = {
	xspf_start_element,
	xspf_end_element,
	xspf_text,
	nullptr,
	nullptr,
};

static void
song_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct song *song = (struct song *)data;

	song_free(song);
}

static void
xspf_parser_destroy(gpointer data)
{
	XspfParser *parser = (XspfParser *)data;

	if (parser->state >= XspfParser::TRACK && parser->song != NULL)
		song_free(parser->song);

	g_slist_foreach(parser->songs, song_free_callback, NULL);
	g_slist_free(parser->songs);
}

/*
 * The playlist object
 *
 */

static struct playlist_provider *
xspf_open_stream(struct input_stream *is)
{
	XspfParser parser;
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
		nbytes = input_stream_lock_read(is, buffer, sizeof(buffer),
						&error);
		if (nbytes == 0) {
			if (error != NULL) {
				g_markup_parse_context_free(context);
				g_warning("%s", error->message);
				g_error_free(error);
				return NULL;
			}

			break;
		}

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

	MemoryPlaylistProvider *playlist =
		new MemoryPlaylistProvider(g_slist_reverse(parser.songs));
	parser.songs = NULL;

	g_markup_parse_context_free(context);

	return playlist;
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
	"xspf",

	nullptr,
	nullptr,
	nullptr,
	xspf_open_stream,
	nullptr,
	nullptr,

	nullptr,
	xspf_suffixes,
	xspf_mime_types,
};
