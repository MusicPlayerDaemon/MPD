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

#include "config.h"
#include "SongFilter.hxx"
#include "Song.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define LOCATE_TAG_FILE_KEY     "file"
#define LOCATE_TAG_FILE_KEY_OLD "filename"
#define LOCATE_TAG_ANY_KEY      "any"

unsigned
locate_parse_type(const char *str)
{
	if (StringEqualsCaseASCII(str, LOCATE_TAG_FILE_KEY) ||
	    StringEqualsCaseASCII(str, LOCATE_TAG_FILE_KEY_OLD))
		return LOCATE_TAG_FILE_TYPE;

	if (StringEqualsCaseASCII(str, LOCATE_TAG_ANY_KEY))
		return LOCATE_TAG_ANY_TYPE;

	return tag_name_parse_i(str);
}

SongFilter::Item::Item(unsigned _tag, const char *_value, bool _fold_case)
	:tag(_tag), fold_case(_fold_case),
	 value(fold_case
	       ? g_utf8_casefold(_value, -1)
	       : g_strdup(_value))
{
}

SongFilter::Item::~Item()
{
	g_free(value);
}

bool
SongFilter::Item::StringMatch(const char *s) const
{
	assert(value != nullptr);
	assert(s != nullptr);

	if (fold_case) {
		char *p = g_utf8_casefold(s, -1);
		const bool result = strstr(p, value) != NULL;
		g_free(p);
		return result;
	} else {
		return strcmp(s, value) == 0;
	}
}

bool
SongFilter::Item::Match(const TagItem &item) const
{
	return (tag == LOCATE_TAG_ANY_TYPE || (unsigned)item.type == tag) &&
		StringMatch(item.value);
}

bool
SongFilter::Item::Match(const Tag &_tag) const
{
	bool visited_types[TAG_NUM_OF_ITEM_TYPES];
	std::fill_n(visited_types, TAG_NUM_OF_ITEM_TYPES, false);

	for (unsigned i = 0; i < _tag.num_items; i++) {
		visited_types[_tag.items[i]->type] = true;

		if (Match(*_tag.items[i]))
			return true;
	}

	if (tag < TAG_NUM_OF_ITEM_TYPES && !visited_types[tag]) {
		/* If the search critieron was not visited during the
		   sweep through the song's tag, it means this field
		   is absent from the tag or empty. Thus, if the
		   searched string is also empty (first char is a \0),
		   then it's a match as well and we should return
		   true. */
		if (*value == 0)
			return true;

		if (tag == TAG_ALBUM_ARTIST && visited_types[TAG_ARTIST]) {
			/* if we're looking for "album artist", but
			   only "artist" exists, use that */
			for (unsigned i = 0; i < _tag.num_items; i++) {
				const TagItem &item = *_tag.items[i];
				if (item.type == TAG_ARTIST &&
				    StringMatch(item.value))
					return true;
			}
		}
	}

	return false;
}

bool
SongFilter::Item::Match(const Song &song) const
{
	if (tag == LOCATE_TAG_FILE_TYPE || tag == LOCATE_TAG_ANY_TYPE) {
		const auto uri = song.GetURI();
		const bool result = StringMatch(uri.c_str());

		if (result || tag == LOCATE_TAG_FILE_TYPE)
			return result;
	}

	return song.tag != NULL && Match(*song.tag);
}

SongFilter::SongFilter(unsigned tag, const char *value, bool fold_case)
{
	items.push_back(Item(tag, value, fold_case));
}

SongFilter::~SongFilter()
{
	/* this destructor exists here just so it won't get inlined */
}

bool
SongFilter::Parse(const char *tag_string, const char *value, bool fold_case)
{
	unsigned tag = locate_parse_type(tag_string);
	if (tag == TAG_NUM_OF_ITEM_TYPES)
		return false;

	items.push_back(Item(tag, value, fold_case));
	return true;
}

bool
SongFilter::Parse(unsigned argc, char *argv[], bool fold_case)
{
	if (argc == 0 || argc % 2 != 0)
		return false;

	for (unsigned i = 0; i < argc; i += 2)
		if (!Parse(argv[i], argv[i + 1], fold_case))
			return false;

	return true;
}

bool
SongFilter::Match(const Song &song) const
{
	for (const auto &i : items)
		if (!i.Match(song))
			return false;

	return true;
}
