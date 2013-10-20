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
#include "PlaylistPlugin.hxx"
#include "MemorySongEnumerator.hxx"
#include "InputStream.hxx"
#include "tag/Tag.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

static constexpr Domain xspf_domain("xspf");

/**
 * This is the state object for the GLib XML parser.
 */
struct XspfParser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	std::forward_list<SongPointer> songs;

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
	TagType tag;

	/**
	 * The current song.  It is allocated after the "location"
	 * element.
	 */
	Song *song;

	XspfParser()
		:state(ROOT) {}
};

static void
xspf_start_element(gcc_unused GMarkupParseContext *context,
		   const gchar *element_name,
		   gcc_unused const gchar **attribute_names,
		   gcc_unused const gchar **attribute_values,
		   gpointer user_data, gcc_unused GError **error)
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
xspf_end_element(gcc_unused GMarkupParseContext *context,
		 const gchar *element_name,
		 gpointer user_data, gcc_unused GError **error)
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
				parser->songs.emplace_front(parser->song);

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
xspf_text(gcc_unused GMarkupParseContext *context,
	  const gchar *text, gsize text_len,
	  gpointer user_data, gcc_unused GError **error)
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
				parser->song->tag = new Tag();
			parser->song->tag->AddItem(parser->tag, text, text_len);
		}

		break;

	case XspfParser::LOCATION:
		if (parser->song == NULL) {
			char *uri = g_strndup(text, text_len);
			parser->song = Song::NewRemote(uri);
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
xspf_parser_destroy(gpointer data)
{
	XspfParser *parser = (XspfParser *)data;

	if (parser->state >= XspfParser::TRACK && parser->song != NULL)
		parser->song->Free();
}

/*
 * The playlist object
 *
 */

static SongEnumerator *
xspf_open_stream(struct input_stream *is)
{
	XspfParser parser;
	GMarkupParseContext *context;
	char buffer[1024];
	size_t nbytes;
	bool success;
	Error error2;
	GError *error = NULL;

	/* parse the XSPF XML file */

	context = g_markup_parse_context_new(&xspf_parser,
					     G_MARKUP_TREAT_CDATA_AS_TEXT,
					     &parser, xspf_parser_destroy);

	while (true) {
		nbytes = is->LockRead(buffer, sizeof(buffer), error2);
		if (nbytes == 0) {
			if (error2.IsDefined()) {
				g_markup_parse_context_free(context);
				LogError(error2);
				return NULL;
			}

			break;
		}

		success = g_markup_parse_context_parse(context, buffer, nbytes,
						       &error);
		if (!success) {
			FormatError(xspf_domain,
				    "XML parser failed: %s", error->message);
			g_error_free(error);
			g_markup_parse_context_free(context);
			return NULL;
		}
	}

	success = g_markup_parse_context_end_parse(context, &error);
	if (!success) {
		FormatError(xspf_domain,
			    "XML parser failed: %s", error->message);
		g_error_free(error);
		g_markup_parse_context_free(context);
		return NULL;
	}

	parser.songs.reverse();
	MemorySongEnumerator *playlist =
		new MemorySongEnumerator(std::move(parser.songs));

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
	xspf_suffixes,
	xspf_mime_types,
};
