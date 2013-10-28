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
#include "AsxPlaylistPlugin.hxx"
#include "PlaylistPlugin.hxx"
#include "MemorySongEnumerator.hxx"
#include "InputStream.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

static constexpr Domain asx_domain("asx");

/**
 * This is the state object for the GLib XML parser.
 */
struct AsxParser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	std::forward_list<SongPointer> songs;

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
	TagType tag;

	/**
	 * The current song.  It is allocated after the "location"
	 * element.
	 */
	Song *song;

	AsxParser()
		:state(ROOT) {}

};

static const gchar *
get_attribute(const gchar **attribute_names, const gchar **attribute_values,
	      const gchar *name)
{
	for (unsigned i = 0; attribute_names[i] != nullptr; ++i)
		if (StringEqualsCaseASCII(attribute_names[i], name))
			return attribute_values[i];

	return nullptr;
}

static void
asx_start_element(gcc_unused GMarkupParseContext *context,
		  const gchar *element_name,
		  const gchar **attribute_names,
		  const gchar **attribute_values,
		  gpointer user_data, gcc_unused GError **error)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		if (StringEqualsCaseASCII(element_name, "entry")) {
			parser->state = AsxParser::ENTRY;
			parser->song = Song::NewRemote("asx:");
			parser->tag = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case AsxParser::ENTRY:
		if (StringEqualsCaseASCII(element_name, "ref")) {
			const gchar *href = get_attribute(attribute_names,
							  attribute_values,
							  "href");
			if (href != nullptr) {
				/* create new song object, and copy
				   the existing tag over; we cannot
				   replace the existing song's URI,
				   because that attribute is
				   immutable */
				Song *song = Song::NewRemote(href);

				if (parser->song != nullptr) {
					song->tag = parser->song->tag;
					parser->song->tag = nullptr;
					parser->song->Free();
				}

				parser->song = song;
			}
		} else if (StringEqualsCaseASCII(element_name, "author"))
			/* is that correct?  or should it be COMPOSER
			   or PERFORMER? */
			parser->tag = TAG_ARTIST;
		else if (StringEqualsCaseASCII(element_name, "title"))
			parser->tag = TAG_TITLE;

		break;
	}
}

static void
asx_end_element(gcc_unused GMarkupParseContext *context,
		const gchar *element_name,
		gpointer user_data, gcc_unused GError **error)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		break;

	case AsxParser::ENTRY:
		if (StringEqualsCaseASCII(element_name, "entry")) {
			if (strcmp(parser->song->uri, "asx:") != 0)
				parser->songs.emplace_front(parser->song);
			else
				parser->song->Free();

			parser->state = AsxParser::ROOT;
		} else
			parser->tag = TAG_NUM_OF_ITEM_TYPES;

		break;
	}
}

static void
asx_text(gcc_unused GMarkupParseContext *context,
	 const gchar *text, gsize text_len,
	 gpointer user_data, gcc_unused GError **error)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		break;

	case AsxParser::ENTRY:
		if (parser->tag != TAG_NUM_OF_ITEM_TYPES) {
			if (parser->song->tag == nullptr)
				parser->song->tag = new Tag();
			parser->song->tag->AddItem(parser->tag,
						   text, text_len);
		}

		break;
	}
}

static const GMarkupParser asx_parser = {
	asx_start_element,
	asx_end_element,
	asx_text,
	nullptr,
	nullptr,
};

static void
asx_parser_destroy(gpointer data)
{
	AsxParser *parser = (AsxParser *)data;

	if (parser->state >= AsxParser::ENTRY)
		parser->song->Free();
}

/*
 * The playlist object
 *
 */

static SongEnumerator *
asx_open_stream(InputStream &is)
{
	AsxParser parser;
	GMarkupParseContext *context;
	char buffer[1024];
	size_t nbytes;
	bool success;
	Error error2;
	GError *error = nullptr;

	/* parse the ASX XML file */

	context = g_markup_parse_context_new(&asx_parser,
					     G_MARKUP_TREAT_CDATA_AS_TEXT,
					     &parser, asx_parser_destroy);

	while (true) {
		nbytes = is.LockRead(buffer, sizeof(buffer), error2);
		if (nbytes == 0) {
			if (error2.IsDefined()) {
				g_markup_parse_context_free(context);
				LogError(error2);
				return nullptr;
			}

			break;
		}

		success = g_markup_parse_context_parse(context, buffer, nbytes,
						       &error);
		if (!success) {
			FormatErrno(asx_domain,
				    "XML parser failed: %s", error->message);
			g_error_free(error);
			g_markup_parse_context_free(context);
			return nullptr;
		}
	}

	success = g_markup_parse_context_end_parse(context, &error);
	if (!success) {
		FormatErrno(asx_domain,
			    "XML parser failed: %s", error->message);
		g_error_free(error);
		g_markup_parse_context_free(context);
		return nullptr;
	}

	parser.songs.reverse();
	MemorySongEnumerator *playlist =
		new MemorySongEnumerator(std::move(parser.songs));

	g_markup_parse_context_free(context);

	return playlist;
}

static const char *const asx_suffixes[] = {
	"asx",
	nullptr
};

static const char *const asx_mime_types[] = {
	"video/x-ms-asf",
	nullptr
};

const struct playlist_plugin asx_playlist_plugin = {
	"asx",

	nullptr,
	nullptr,
	nullptr,
	asx_open_stream,

	nullptr,
	asx_suffixes,
	asx_mime_types,
};
