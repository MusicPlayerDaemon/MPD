/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "AsxPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "tag/Builder.hxx"
#include "util/ASCII.hxx"
#include "util/StringView.hxx"
#include "lib/expat/ExpatParser.hxx"

/**
 * This is the state object for our XML parser.
 */
struct AsxParser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	std::forward_list<DetachedSong> songs;

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
	TagType tag_type;

	/**
	 * The current song URI.  It is set by the "ref" element.
	 */
	std::string location;

	TagBuilder tag_builder;

	AsxParser()
		:state(ROOT) {}

};

static void XMLCALL
asx_start_element(void *user_data, const XML_Char *element_name,
		  const XML_Char **atts)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		if (StringEqualsCaseASCII(element_name, "entry")) {
			parser->state = AsxParser::ENTRY;
			parser->location.clear();
			parser->tag_type = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case AsxParser::ENTRY:
		if (StringEqualsCaseASCII(element_name, "ref")) {
			const char *href =
				ExpatParser::GetAttributeCase(atts, "href");
			if (href != nullptr)
				parser->location = href;
		} else if (StringEqualsCaseASCII(element_name, "author"))
			/* is that correct?  or should it be COMPOSER
			   or PERFORMER? */
			parser->tag_type = TAG_ARTIST;
		else if (StringEqualsCaseASCII(element_name, "title"))
			parser->tag_type = TAG_TITLE;

		break;
	}
}

static void XMLCALL
asx_end_element(void *user_data, const XML_Char *element_name)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		break;

	case AsxParser::ENTRY:
		if (StringEqualsCaseASCII(element_name, "entry")) {
			if (!parser->location.empty())
				parser->songs.emplace_front(std::move(parser->location),
							    parser->tag_builder.Commit());

			parser->state = AsxParser::ROOT;
		} else
			parser->tag_type = TAG_NUM_OF_ITEM_TYPES;

		break;
	}
}

static void XMLCALL
asx_char_data(void *user_data, const XML_Char *s, int len)
{
	AsxParser *parser = (AsxParser *)user_data;

	switch (parser->state) {
	case AsxParser::ROOT:
		break;

	case AsxParser::ENTRY:
		if (parser->tag_type != TAG_NUM_OF_ITEM_TYPES)
			parser->tag_builder.AddItem(parser->tag_type,
						    StringView(s, len));

		break;
	}
}

/*
 * The playlist object
 *
 */

static std::unique_ptr<SongEnumerator>
asx_open_stream(InputStreamPtr &&is)
{
	AsxParser parser;

	{
		ExpatParser expat(&parser);
		expat.SetElementHandler(asx_start_element, asx_end_element);
		expat.SetCharacterDataHandler(asx_char_data);
		expat.Parse(*is);
	}

	parser.songs.reverse();
	return std::make_unique<MemorySongEnumerator>(std::move(parser.songs));
}

static const char *const asx_suffixes[] = {
	"asx",
	nullptr
};

static const char *const asx_mime_types[] = {
	"video/x-ms-asf",
	nullptr
};

const PlaylistPlugin asx_playlist_plugin =
	PlaylistPlugin("asx", asx_open_stream)
	.WithSuffixes(asx_suffixes)
	.WithMimeTypes(asx_mime_types);
