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
#include "path.h"
#include "song.h"

extern "C" {
#include "tag.h"
}

#include <glib.h>

#include <assert.h>
#include <stdlib.h>

#define LOCATE_TAG_FILE_KEY     "file"
#define LOCATE_TAG_FILE_KEY_OLD "filename"
#define LOCATE_TAG_ANY_KEY      "any"

/* struct used for search, find, list queries */
struct locate_item {
	uint8_t tag;

	bool fold_case;

	/* what we are looking for */
	char *needle;
};

/**
 * An array of struct locate_item objects.
 */
struct locate_item_list {
	/** number of items */
	unsigned length;

	/** this is a variable length array */
	struct locate_item items[1];
};

unsigned
locate_parse_type(const char *str)
{
	if (0 == g_ascii_strcasecmp(str, LOCATE_TAG_FILE_KEY) ||
	    0 == g_ascii_strcasecmp(str, LOCATE_TAG_FILE_KEY_OLD))
		return LOCATE_TAG_FILE_TYPE;

	if (0 == g_ascii_strcasecmp(str, LOCATE_TAG_ANY_KEY))
		return LOCATE_TAG_ANY_TYPE;

	return tag_name_parse_i(str);
}

static bool
locate_item_init(struct locate_item *item,
		 const char *type_string, const char *needle,
		 bool fold_case)
{
	item->tag = locate_parse_type(type_string);

	if (item->tag == TAG_NUM_OF_ITEM_TYPES)
		return false;

	item->fold_case = fold_case;
	item->needle = fold_case
		? g_utf8_casefold(needle, -1)
		: g_strdup(needle);

	return true;
}

void
locate_item_list_free(struct locate_item_list *list)
{
	for (unsigned i = 0; i < list->length; ++i)
		g_free(list->items[i].needle);

	g_free(list);
}

static struct locate_item_list *
locate_item_list_new(unsigned length)
{
	struct locate_item_list *list = (struct locate_item_list *)
		g_malloc(sizeof(*list) - sizeof(list->items[0]) +
			 length * sizeof(list->items[0]));
	list->length = length;

	return list;
}

struct locate_item_list *
locate_item_list_new_single(unsigned tag, const char *needle)
{
	struct locate_item_list *list = locate_item_list_new(1);
	list->items[0].tag = tag;
	list->items[0].fold_case = false;
	list->items[0].needle = g_strdup(needle);
	return list;
}

struct locate_item_list *
locate_item_list_parse(char *argv[], unsigned argc, bool fold_case)
{
	if (argc == 0 || argc % 2 != 0)
		return NULL;

	struct locate_item_list *list = locate_item_list_new(argc / 2);

	for (unsigned i = 0; i < list->length; ++i) {
		if (!locate_item_init(&list->items[i], argv[i * 2],
				      argv[i * 2 + 1], fold_case)) {
			locate_item_list_free(list);
			return NULL;
		}
	}

	return list;
}

gcc_pure
static bool
locate_string_match(const struct locate_item *item, const char *value)
{
	assert(item != NULL);
	assert(value != NULL);

	if (item->fold_case) {
		char *p = g_utf8_casefold(value, -1);
		const bool result = strstr(p, item->needle) != NULL;
		g_free(p);
		return result;
	} else {
		return strcmp(value, item->needle) == 0;
	}
}

gcc_pure
static bool
locate_tag_match(const struct locate_item *item, const struct tag *tag)
{
	assert(item != NULL);
	assert(tag != NULL);

	bool visited_types[TAG_NUM_OF_ITEM_TYPES];
	memset(visited_types, 0, sizeof(visited_types));

	for (unsigned i = 0; i < tag->num_items; i++) {
		visited_types[tag->items[i]->type] = true;
		if (item->tag != LOCATE_TAG_ANY_TYPE &&
		    tag->items[i]->type != item->tag)
			continue;

		if (locate_string_match(item, tag->items[i]->value))
			return true;
	}

	/** If the search critieron was not visited during the sweep
	 * through the song's tag, it means this field is absent from
	 * the tag or empty. Thus, if the searched string is also
	 *  empty (first char is a \0), then it's a match as well and
	 *  we should return true.
	 */
	if (*item->needle == 0 && item->tag != LOCATE_TAG_ANY_TYPE &&
	    !visited_types[item->tag])
		return true;

	return false;
}

gcc_pure
static bool
locate_song_match(const struct locate_item *item, const struct song *song)
{
	if (item->tag == LOCATE_TAG_FILE_TYPE ||
	    item->tag == LOCATE_TAG_ANY_TYPE) {
		char *uri = song_get_uri(song);
		const bool result = locate_string_match(item, uri);
		g_free(uri);

		if (result || item->tag == LOCATE_TAG_FILE_TYPE)
			return result;
	}

	return song->tag != NULL && locate_tag_match(item, song->tag);
}

bool
locate_list_song_match(const struct song *song,
		       const struct locate_item_list *criteria)
{
	for (unsigned i = 0; i < criteria->length; i++)
		if (!locate_song_match(&criteria->items[i], song))
			return false;

	return true;
}
