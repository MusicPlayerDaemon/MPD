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
#include "locate.h"
#include "path.h"
#include "tag.h"
#include "song.h"

#include <glib.h>

#include <stdlib.h>

#define LOCATE_TAG_FILE_KEY     "file"
#define LOCATE_TAG_FILE_KEY_OLD "filename"
#define LOCATE_TAG_ANY_KEY      "any"

int
locate_parse_type(const char *str)
{
	int i;

	if (0 == g_ascii_strcasecmp(str, LOCATE_TAG_FILE_KEY) ||
	    0 == g_ascii_strcasecmp(str, LOCATE_TAG_FILE_KEY_OLD))
		return LOCATE_TAG_FILE_TYPE;

	if (0 == g_ascii_strcasecmp(str, LOCATE_TAG_ANY_KEY))
		return LOCATE_TAG_ANY_TYPE;

	i = tag_name_parse_i(str);
	if (i != TAG_NUM_OF_ITEM_TYPES)
		return i;

	return -1;
}

static bool
locate_item_init(struct locate_item *item,
		 const char *type_string, const char *needle)
{
	item->tag = locate_parse_type(type_string);

	if (item->tag < 0)
		return false;

	item->needle = g_strdup(needle);

	return true;
}

struct locate_item *
locate_item_new(const char *type_string, const char *needle)
{
	struct locate_item *ret = g_new(struct locate_item, 1);

	if (!locate_item_init(ret, type_string, needle)) {
		g_free(ret);
		ret = NULL;
	}

	return ret;
}

void
locate_item_list_free(struct locate_item_list *list)
{
	for (unsigned i = 0; i < list->length; ++i)
		g_free(list->items[i].needle);

	g_free(list);
}

struct locate_item_list *
locate_item_list_new(unsigned length)
{
	struct locate_item_list *list;

	list = g_malloc0(sizeof(*list) - sizeof(list->items[0]) +
			 length * sizeof(list->items[0]));
	list->length = length;

	return list;
}

struct locate_item_list *
locate_item_list_parse(char *argv[], int argc)
{
	struct locate_item_list *list;

	if (argc % 2 != 0)
		return NULL;

	list = locate_item_list_new(argc / 2);

	for (unsigned i = 0; i < list->length; ++i) {
		if (!locate_item_init(&list->items[i], argv[i * 2],
				      argv[i * 2 + 1])) {
			locate_item_list_free(list);
			return NULL;
		}
	}

	return list;
}

struct locate_item_list *
locate_item_list_casefold(const struct locate_item_list *list)
{
	struct locate_item_list *new_list = locate_item_list_new(list->length);

	for (unsigned i = 0; i < list->length; i++){
		new_list->items[i].needle =
			g_utf8_casefold(list->items[i].needle, -1);
		new_list->items[i].tag = list->items[i].tag;
	}

	return new_list;
}

void
locate_item_free(struct locate_item *item)
{
	g_free(item->needle);
	g_free(item);
}

static bool
locate_tag_search(const struct song *song, enum tag_type type, const char *str)
{
	char *duplicate;
	bool ret = false;
	bool visited_types[TAG_NUM_OF_ITEM_TYPES];

	if (type == LOCATE_TAG_FILE_TYPE || type == LOCATE_TAG_ANY_TYPE) {
		char *uri, *p;

		uri = song_get_uri(song);
		p = g_utf8_casefold(uri, -1);
		g_free(uri);

		if (strstr(p, str))
			ret = true;
		g_free(p);
		if (ret == 1 || type == LOCATE_TAG_FILE_TYPE)
			return ret;
	}

	if (!song->tag)
		return false;

	memset(visited_types, 0, sizeof(visited_types));

	for (unsigned i = 0; i < song->tag->num_items && !ret; i++) {
		visited_types[song->tag->items[i]->type] = true;
		if (type != LOCATE_TAG_ANY_TYPE &&
		    song->tag->items[i]->type != type) {
			continue;
		}

		duplicate = g_utf8_casefold(song->tag->items[i]->value, -1);
		if (*str && strstr(duplicate, str))
			ret = true;
		g_free(duplicate);
	}

	/** If the search critieron was not visited during the sweep
	 * through the song's tag, it means this field is absent from
	 * the tag or empty. Thus, if the searched string is also
	 *  empty (first char is a \0), then it's a match as well and
	 *  we should return true.
	 */
	if (!*str && !visited_types[type])
		return true;

	return ret;
}

bool
locate_song_search(const struct song *song,
		   const struct locate_item_list *criteria)
{
	for (unsigned i = 0; i < criteria->length; i++)
		if (!locate_tag_search(song, criteria->items[i].tag,
				       criteria->items[i].needle))
			return false;

	return true;
}

static bool
locate_tag_match(const struct song *song, enum tag_type type, const char *str)
{
	bool visited_types[TAG_NUM_OF_ITEM_TYPES];

	if (type == LOCATE_TAG_FILE_TYPE || type == LOCATE_TAG_ANY_TYPE) {
		char *uri = song_get_uri(song);
		bool matches = strcmp(str, uri) == 0;
		g_free(uri);

		if (matches)
			return true;

		if (type == LOCATE_TAG_FILE_TYPE)
			return false;
	}

	if (!song->tag)
		return false;

	memset(visited_types, 0, sizeof(visited_types));

	for (unsigned i = 0; i < song->tag->num_items; i++) {
		visited_types[song->tag->items[i]->type] = true;
		if (type != LOCATE_TAG_ANY_TYPE &&
		    song->tag->items[i]->type != type) {
			continue;
		}

		if (0 == strcmp(str, song->tag->items[i]->value))
			return true;
	}

	/** If the search critieron was not visited during the sweep
	 * through the song's tag, it means this field is absent from
	 * the tag or empty. Thus, if the searched string is also
	 *  empty (first char is a \0), then it's a match as well and
	 *  we should return true.
	 */
	if (!*str && !visited_types[type])
		return true;

	return false;
}

bool
locate_song_match(const struct song *song,
		  const struct locate_item_list *criteria)
{
	for (unsigned i = 0; i < criteria->length; i++)
		if (!locate_tag_match(song, criteria->items[i].tag,
				      criteria->items[i].needle))
			return false;

	return true;
}
