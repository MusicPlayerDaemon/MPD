/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "playlist/rss_playlist_plugin.h"
#include "playlist_plugin.h"
#include "input_stream.h"
#include "song.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "rss"

/**
 * This is the state object for the GLib XML parser.
 */
struct rss_parser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	GSList *songs;

	/**
	 * The current position in the XML file.
	 */
	enum {
		ROOT, ITEM,
	} state;

	/**
	 * The current tag within the "entry" element.  This is only
	 * valid if state==ITEM.  TAG_NUM_OF_ITEM_TYPES means there
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
rss_start_element(G_GNUC_UNUSED GMarkupParseContext *context,
		  const gchar *element_name,
		  const gchar **attribute_names,
		  const gchar **attribute_values,
		  gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct rss_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		if (g_ascii_strcasecmp(element_name, "item") == 0) {
			parser->state = ITEM;
			parser->song = song_remote_new("rss:");
			parser->tag = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case ITEM:
		if (g_ascii_strcasecmp(element_name, "enclosure") == 0) {
			const gchar *href = get_attribute(attribute_names,
							  attribute_values,
							  "url");
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
		} else if (g_ascii_strcasecmp(element_name, "title") == 0)
			parser->tag = TAG_TITLE;
		else if (g_ascii_strcasecmp(element_name, "itunes:author") == 0)
			parser->tag = TAG_ARTIST;

		break;
	}
}

static void
rss_end_element(G_GNUC_UNUSED GMarkupParseContext *context,
		const gchar *element_name,
		gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct rss_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		break;

	case ITEM:
		if (g_ascii_strcasecmp(element_name, "item") == 0) {
			if (strcmp(parser->song->uri, "rss:") != 0)
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
rss_text(G_GNUC_UNUSED GMarkupParseContext *context,
	 const gchar *text, gsize text_len,
	 gpointer user_data, G_GNUC_UNUSED GError **error)
{
	struct rss_parser *parser = user_data;

	switch (parser->state) {
	case ROOT:
		break;

	case ITEM:
		if (parser->tag != TAG_NUM_OF_ITEM_TYPES) {
			if (parser->song->tag == NULL)
				parser->song->tag = tag_new();
			tag_add_item_n(parser->song->tag, parser->tag,
				       text, text_len);
		}

		break;
	}
}

static const GMarkupParser rss_parser = {
	.start_element = rss_start_element,
	.end_element = rss_end_element,
	.text = rss_text,
};

static void
song_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct song *song = data;

	song_free(song);
}

static void
rss_parser_destroy(gpointer data)
{
	struct rss_parser *parser = data;

	if (parser->state >= ITEM)
		song_free(parser->song);

	g_slist_foreach(parser->songs, song_free_callback, NULL);
	g_slist_free(parser->songs);
}

/*
 * The playlist object
 *
 */

struct rss_playlist {
	struct playlist_provider base;

	GSList *songs;
};

static struct playlist_provider *
rss_open_stream(struct input_stream *is)
{
	struct rss_parser parser = {
		.songs = NULL,
		.state = ROOT,
	};
	struct rss_playlist *playlist;
	GMarkupParseContext *context;
	char buffer[1024];
	size_t nbytes;
	bool success;
	GError *error = NULL;

	/* parse the RSS XML file */

	context = g_markup_parse_context_new(&rss_parser,
					     G_MARKUP_TREAT_CDATA_AS_TEXT,
					     &parser, rss_parser_destroy);

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

	/* create a #rss_playlist object from the parsed song list */

	playlist = g_new(struct rss_playlist, 1);
	playlist_provider_init(&playlist->base, &rss_playlist_plugin);
	playlist->songs = g_slist_reverse(parser.songs);
	parser.songs = NULL;

	g_markup_parse_context_free(context);

	return &playlist->base;
}

static void
rss_close(struct playlist_provider *_playlist)
{
	struct rss_playlist *playlist = (struct rss_playlist *)_playlist;

	g_slist_foreach(playlist->songs, song_free_callback, NULL);
	g_slist_free(playlist->songs);
	g_free(playlist);
}

static struct song *
rss_read(struct playlist_provider *_playlist)
{
	struct rss_playlist *playlist = (struct rss_playlist *)_playlist;
	struct song *song;

	if (playlist->songs == NULL)
		return NULL;

	song = playlist->songs->data;
	playlist->songs = g_slist_remove(playlist->songs, song);

	return song;
}

static const char *const rss_suffixes[] = {
	"rss",
	NULL
};

static const char *const rss_mime_types[] = {
	"application/rss+xml",
	"text/xml",
	NULL
};

const struct playlist_plugin rss_playlist_plugin = {
	.name = "rss",

	.open_stream = rss_open_stream,
	.close = rss_close,
	.read = rss_read,

	.suffixes = rss_suffixes,
	.mime_types = rss_mime_types,
};
