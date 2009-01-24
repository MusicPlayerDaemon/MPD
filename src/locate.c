/* the Music Player Daemon (MPD)
 * (c)2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

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

	if (0 == strcasecmp(str, LOCATE_TAG_FILE_KEY) ||
	    0 == strcasecmp(str, LOCATE_TAG_FILE_KEY_OLD))
		return LOCATE_TAG_FILE_TYPE;

	if (0 == strcasecmp(str, LOCATE_TAG_ANY_KEY))
		return LOCATE_TAG_ANY_TYPE;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++)
		if (0 == strcasecmp(str, mpdTagItemKeys[i]))
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
		free(ret);
		ret = NULL;
	}

	return ret;
}

void
locate_item_list_free(int count, struct locate_item *array)
{
	int i;

	for (i = 0; i < count; i++)
		free(array[i].needle);

	free(array);
}

int
locate_item_list_parse(char *argv[], int argc, struct locate_item **arrayRet)
{
	int i, j;
	struct locate_item *item;

	if (argc == 0)
		return 0;

	if (argc % 2 != 0)
		return -1;

	*arrayRet = g_new(struct locate_item, argc / 2);

	for (i = 0, item = *arrayRet; i < argc / 2; i++, item++) {
		if (!locate_item_init(item, argv[i * 2], argv[i * 2 + 1]))
			goto fail;
	}

	return argc / 2;

fail:
	for (j = 0; j < i; j++) {
		free((*arrayRet)[j].needle);
	}

	free(*arrayRet);
	*arrayRet = NULL;
	return -1;
}

void
locate_item_free(struct locate_item *item)
{
	free(item->needle);
	free(item);
}

static bool
locate_tag_search(const struct song *song, enum tag_type type, const char *str)
{
	int i;
	char *duplicate;
	bool ret = false;
	bool visited_types[TAG_NUM_OF_ITEM_TYPES] = { false };

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

	for (i = 0; i < song->tag->numOfItems && !ret; i++) {
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
locate_song_search(const struct song *song, int num_items,
		   const struct locate_item *items)
{
	for (int i = 0; i < num_items; i++)
		if (!locate_tag_search(song, items[i].tag, items[i].needle))
			return false;

	return true;
}

static bool
locate_tag_match(const struct song *song, enum tag_type type, const char *str)
{
	int i;
	bool visited_types[TAG_NUM_OF_ITEM_TYPES] = { false };

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

	for (i = 0; i < song->tag->numOfItems; i++) {
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
locate_song_match(const struct song *song, int num_items,
		  const struct locate_item *items)
{
	for (int i = 0; i < num_items; i++)
		if (!locate_tag_match(song, items[i].tag, items[i].needle))
			return false;

	return true;
}
