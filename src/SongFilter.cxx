/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "DetachedSong.hxx"
#include "tag/Tag.hxx"
#include "util/ASCII.hxx"
#include "util/UriUtil.hxx"

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

	if (strcmp(str, "base") == 0)
		return LOCATE_TAG_BASE_TYPE;

	return tag_name_parse_i(str);
}

gcc_pure
static std::string
CaseFold(const char *p)
{
	char *q = g_utf8_casefold(p, -1);
	std::string result(q);
	g_free(q);
	return result;
}

gcc_pure
static std::string
ImportString(const char *p, bool fold_case)
{
	return fold_case
		? CaseFold(p)
		: std::string(p);
}

SongFilter::Item::Item(unsigned _tag, const char *_value, bool _fold_case)
	:tag(_tag), fold_case(_fold_case),
	 value(ImportString(_value, _fold_case))
{
}

bool
SongFilter::Item::StringMatch(const char *s) const
{
	assert(s != nullptr);

	if (fold_case) {
		char *p = g_utf8_casefold(s, -1);
		const bool result = strstr(p, value.c_str()) != NULL;
		g_free(p);
		return result;
	} else {
		return s == value;
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
	std::fill_n(visited_types, size_t(TAG_NUM_OF_ITEM_TYPES), false);

	for (unsigned i = 0; i < _tag.num_items; i++) {
		visited_types[_tag.items[i]->type] = true;

		if (Match(*_tag.items[i]))
			return true;
	}

	if (tag < TAG_NUM_OF_ITEM_TYPES && !visited_types[tag]) {
		/* If the search critieron was not visited during the
		   sweep through the song's tag, it means this field
		   is absent from the tag or empty. Thus, if the
		   searched string is also empty
		   then it's a match as well and we should return
		   true. */
		if (value.empty())
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
	if (tag == LOCATE_TAG_BASE_TYPE) {
		const auto uri = song.GetURI();
		return uri_is_child_or_same(value.c_str(), uri.c_str());
	}

	if (tag == LOCATE_TAG_FILE_TYPE) {
		const auto uri = song.GetURI();
		return StringMatch(uri.c_str());
	}

	return song.tag != NULL && Match(*song.tag);
}

bool
SongFilter::Item::Match(const DetachedSong &song) const
{
	if (tag == LOCATE_TAG_BASE_TYPE)
		return uri_is_child_or_same(value.c_str(), song.GetURI());

	if (tag == LOCATE_TAG_FILE_TYPE)
		return StringMatch(song.GetURI());

	return Match(song.GetTag());
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

	if (tag == LOCATE_TAG_BASE_TYPE) {
		if (!uri_safe_local(value))
			return false;

		/* case folding doesn't work with "base" */
		fold_case = false;
	}

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

bool
SongFilter::Match(const DetachedSong &song) const
{
	for (const auto &i : items)
		if (!i.Match(song))
			return false;

	return true;
}

std::string
SongFilter::GetBase() const
{
	for (const auto &i : items)
		if (i.GetTag() == LOCATE_TAG_BASE_TYPE)
			return i.GetValue();

	return std::string();
}
