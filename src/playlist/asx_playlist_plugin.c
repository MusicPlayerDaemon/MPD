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
#include "playlist/asx_playlist_plugin.h"
#include "playlist_plugin.h"
#include "input_stream.h"
#include "song.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "asx"

/**
 * This is the state object for the GLib XML parser.
 */
struct asx_parser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	GSList *songs;

	/**
	 * The current position in the XML file.
	 */
	enum {
		ROOT, ENTRY,
	} state;

	/**
	 * The current tag within the "entry" element.  This is only
	 * valid if state==ENTRY.  TAG_NUM_OF_ITEM_TYPES means there
	 * is no (known) tag.
	 */
	enum tag_type tag;

	/**
	 * The current song.  It is allocated after the "location"
	 * element.
	 */
	struct song *song;
};

static const gchar *
get_attribute(const gchar **attribute_names, const gchar **attribute_values,
	      const gchar *name)
{
	for (unsigned i = 0; attribute_names[i] != NULL; ++i)
		if (g_ascii_strcasecmp(attribute_names[i], name) == 0)
			return attribute_values[i];

	return NULL;
}

static void
asx_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
		  const gchar *element_name,
		  const gchar **attribute_names,
		  const gchar **attribute_values,
		  gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct asx_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		if (g_ascii_strcasecmp(element_name, "entry") == 0) {
			parser->state = ENTRY;
			parser->song = song_remote_new("asx:");
			parser->tag = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case ENTRY:
		if (g_ascii_strcasecmp(element_name, "ref") == 0) {
			const gchar *href = get_attribute(attribute_names,
							  attribute_values,
							  "href");
			if (href != NULL) {
				/* create new song object, and copy
				   the existing tag over; we cannot
				   replace the existing song's URI,
				   because that attribute is
				   immutable */
				struct song *song = song_remote_new(href);

				if (parser->song != NULL) {
					song->tag = parser->song->tag;
					parser->song->tag = NULL;
					song_free(parser->song);
				}

				parser->song = song;
			}
		} else if (g_ascii_strcasecmp(element_name, "author") == 0)
			/* is that correct?  or should it be COMPOSER
			   or PERFORMER? */
			parser->tag = TAG_ARTIST;
		else if (g_ascii_strcasecmp(element_name, "title") == 0)
			parser->tag = TAG_TITLE;

		break;
	}
}

static void
asx_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
		const gchar *element_name,
		gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct asx_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		break;

	case ENTRY:
		if (g_ascii_strcasecmp(element_name, "entry") == 0) {
			if (strcmp(parser->song->uri, "asx:") != 0)
				parser->songs = g_slist_prepend(parser->songs,
								parser->song);
			else
				song_free(parser->song);

			parser->state = ROOT;
		} else
			parser->tag = TAG_NUM_OF_ITEM_TYPES;

		break;
	}
}

static void
asx_text(G_GNUC_UNUSED GMarkupParseContext *context,
	 const gchar *text, gsize text_len,
	 gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct asx_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		break;

	case ENTRY:
		if (parser->tag != TAG_NUM_OF_ITEM_TYPES) {
			if (parser->song->tag == NULL)
				parser->song->tag = tag_new();
			tag_add_item_n(parser->song->tag, parser->tag,
				       text, text_len);
		}

		break;
	}
}

static const GMarkupParser asx_parser = {
	.start_element = asx_start_element,
	.end_element = asx_end_element,
	.text = asx_text,
};

static void
song_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct song *song = data;

	song_free(song);
}

static void
asx_parser_destroy(gpointer data)
{
	struct asx_parser *parser = data;

	if (parser->state >= ENTRY)
		song_free(parser->song);

	g_slist_foreach(parser->songs, song_free_callback, NULL);
	g_slist_free(parser->songs);
}

/*
 * The playlist object
 *
 */

struct asx_playlist {
	struct playlist_provider base;

	GSList *songs;
};

static struct playlist_provider *
asx_open_stream(struct input_stream *is)
{
	struct asx_parser parser = {
		.songs = NULL,
		.state = ROOT,
	};
	struct asx_playlist *playlist;
	GMarkupParseContext *context;
	char buffer[1024];
	size_t nbytes;
	bool success;
	GError *error = NULL;

	/* parse the ASX XML file */

	context = g_markup_parse_context_new(&asx_parser,
					     G_MARKUP_TREAT_CDATA_AS_TEXT,
					     &parser, asx_parser_destroy);

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

	/* create a #asx_playlist object from the parsed song list */

	playlist = g_new(struct asx_playlist, 1);
	playlist_provider_init(&playlist->base, &asx_playlist_plugin);
	playlist->songs = g_slist_reverse(parser.songs);
	parser.songs = NULL;

	g_markup_parse_context_free(context);

	return &playlist->base;
}

static void
asx_close(struct playlist_provider *_playlist)
{
	struct asx_playlist *playlist = (struct asx_playlist *)_playlist;

	g_slist_foreach(playlist->songs, song_free_callback, NULL);
	g_slist_free(playlist->songs);
	g_free(playlist);
}

static struct song *
asx_read(struct playlist_provider *_playlist)
{
	struct asx_playlist *playlist = (struct asx_playlist *)_playlist;
	struct song *song;

	if (playlist->songs == NULL)
		return NULL;

	song = playlist->songs->data;
	playlist->songs = g_slist_remove(playlist->songs, song);

	return song;
}

static const char *const asx_suffixes[] = {
	"asx",
	NULL
};

static const char *const asx_mime_types[] = {
	"video/x-ms-asf",
	NULL
};

const struct playlist_plugin asx_playlist_plugin = {
	.name = "asx",

	.open_stream = asx_open_stream,
	.close = asx_close,
	.read = asx_read,

	.suffixes = asx_suffixes,
	.mime_types = asx_mime_types,
};
