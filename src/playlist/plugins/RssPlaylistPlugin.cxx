// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "RssPlaylistPlugin.hxx"
#include "../PlaylistPlugin.hxx"
#include "../MemorySongEnumerator.hxx"
#include "tag/Builder.hxx"
#include "util/ASCII.hxx"
#include "lib/expat/ExpatParser.hxx"

/**
 * This is the state object for the our XML parser.
 */
struct RssParser {
	/**
	 * The list of songs (in reverse order because that's faster
	 * while adding).
	 */
	std::forward_list<DetachedSong> songs;

	/**
	 * The current position in the XML file.
	 */
	enum {
		ROOT, ITEM,
	} state{ROOT};

	/**
	 * The current tag within the "entry" element.  This is only
	 * valid if state==ITEM.  TAG_NUM_OF_ITEM_TYPES means there
	 * is no (known) tag.
	 */
	TagType tag_type;

	/**
	 * The current song URI.  It is set by the "enclosure"
	 * element.
	 */
	std::string location;

	TagBuilder tag_builder;

	RssParser() = default;
};

static void XMLCALL
rss_start_element(void *user_data, const XML_Char *element_name,
		  const XML_Char **atts)
{
	auto *parser = (RssParser *)user_data;

	switch (parser->state) {
	case RssParser::ROOT:
		if (StringEqualsCaseASCII(element_name, "item")) {
			parser->state = RssParser::ITEM;
			parser->location.clear();
			parser->tag_type = TAG_NUM_OF_ITEM_TYPES;
		}

		break;

	case RssParser::ITEM:
		if (StringEqualsCaseASCII(element_name, "enclosure")) {
			const char *href =
				ExpatParser::GetAttributeCase(atts, "url");
			if (href != nullptr)
				parser->location = href;
		} else if (StringEqualsCaseASCII(element_name, "title"))
			parser->tag_type = TAG_TITLE;
		else if (StringEqualsCaseASCII(element_name, "itunes:author"))
			parser->tag_type = TAG_ARTIST;

		break;
	}
}

static void XMLCALL
rss_end_element(void *user_data, const XML_Char *element_name)
{
	auto *parser = (RssParser *)user_data;

	switch (parser->state) {
	case RssParser::ROOT:
		break;

	case RssParser::ITEM:
		if (StringEqualsCaseASCII(element_name, "item")) {
			if (!parser->location.empty())
				parser->songs.emplace_front(std::move(parser->location),
							    parser->tag_builder.Commit());

			parser->state = RssParser::ROOT;
		} else
			parser->tag_type = TAG_NUM_OF_ITEM_TYPES;

		break;
	}
}

static void XMLCALL
rss_char_data(void *user_data, const XML_Char *s, int len)
{
	auto *parser = (RssParser *)user_data;

	switch (parser->state) {
	case RssParser::ROOT:
		break;

	case RssParser::ITEM:
		if (parser->tag_type != TAG_NUM_OF_ITEM_TYPES)
			parser->tag_builder.AddItem(parser->tag_type,
						    std::string_view(s, len));

		break;
	}
}

/*
 * The playlist object
 *
 */

static std::unique_ptr<SongEnumerator>
rss_open_stream(InputStreamPtr &&is)
{
	RssParser parser;

	{
		ExpatParser expat(&parser);
		expat.SetElementHandler(rss_start_element, rss_end_element);
		expat.SetCharacterDataHandler(rss_char_data);
		expat.Parse(*is);
	}

	parser.songs.reverse();
	return std::make_unique<MemorySongEnumerator>(std::move(parser.songs));
}

static constexpr const char *rss_suffixes[] = {
	"rss",
	nullptr
};

static constexpr const char *rss_mime_types[] = {
	"application/rss+xml",
	"application/xml",
	"text/xml",
	nullptr
};

const PlaylistPlugin rss_playlist_plugin =
	PlaylistPlugin("rss", rss_open_stream)
	.WithSuffixes(rss_suffixes)
	.WithMimeTypes(rss_mime_types);
