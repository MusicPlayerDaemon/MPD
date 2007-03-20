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

#include "utils.h"

#define LOCATE_TAG_FILE_KEY     "file"
#define LOCATE_TAG_FILE_KEY_OLD "filename"
#define LOCATE_TAG_ANY_KEY      "any"

int getLocateTagItemType(char *str)
{
	int i;

	if (0 == strcasecmp(str, LOCATE_TAG_FILE_KEY) ||
	    0 == strcasecmp(str, LOCATE_TAG_FILE_KEY_OLD)) 
	{
		return LOCATE_TAG_FILE_TYPE;
	}

	if (0 == strcasecmp(str, LOCATE_TAG_ANY_KEY)) 
	{
		return LOCATE_TAG_ANY_TYPE;
	}

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) 
	{
		if (0 == strcasecmp(str, mpdTagItemKeys[i]))
			return i;
	}

	return -1;
}

static int initLocateTagItem(LocateTagItem * item, char *typeStr, char *needle)
{
	item->tagType = getLocateTagItemType(typeStr);

	if (item->tagType < 0)
		return -1;

	item->needle = xstrdup(needle);

	return 0;
}

LocateTagItem *newLocateTagItem(char *typeStr, char *needle)
{
	LocateTagItem *ret = xmalloc(sizeof(LocateTagItem));

	if (initLocateTagItem(ret, typeStr, needle) < 0) {
		free(ret);
		ret = NULL;
	}

	return ret;
}

void freeLocateTagItemArray(int count, LocateTagItem * array)
{
	int i;

	for (i = 0; i < count; i++)
		free(array[i].needle);

	free(array);
}

int newLocateTagItemArrayFromArgArray(char *argArray[],
				      int numArgs, LocateTagItem ** arrayRet)
{
	int i, j;
	LocateTagItem *item;

	if (numArgs == 0)
		return 0;

	if (numArgs % 2 != 0)
		return -1;

	*arrayRet = xmalloc(sizeof(LocateTagItem) * numArgs / 2);

	for (i = 0, item = *arrayRet; i < numArgs / 2; i++, item++) {
		if (initLocateTagItem
		    (item, argArray[i * 2], argArray[i * 2 + 1]) < 0)
			goto fail;
	}

	return numArgs / 2;

fail:
	for (j = 0; j < i; j++) {
		free((*arrayRet)[j].needle);
	}

	free(*arrayRet);
	*arrayRet = NULL;
	return -1;
}

void freeLocateTagItem(LocateTagItem * item)
{
	free(item->needle);
	free(item);
}

static int strstrSearchTag(Song * song, int type, char *str)
{
	int i;
	char *dup;
	int ret = 0;

	if (type == LOCATE_TAG_FILE_TYPE || type == LOCATE_TAG_ANY_TYPE) {
		dup = strDupToUpper(getSongUrl(song));
		if (strstr(dup, str))
			ret = 1;
		free(dup);
		if (ret == 1 || type == LOCATE_TAG_FILE_TYPE) {
			return ret;
		}
	}

	if (!song->tag)
		return 0;

	for (i = 0; i < song->tag->numOfItems && !ret; i++) {
		if (type != LOCATE_TAG_ANY_TYPE &&
		    song->tag->items[i].type != type) {
			continue;
		}

		dup = strDupToUpper(song->tag->items[i].value);
		if (strstr(dup, str))
			ret = 1;
		free(dup);
	}

	return ret;
}

int strstrSearchTags(Song * song, int numItems, LocateTagItem * items)
{
	int i;

	for (i = 0; i < numItems; i++) {
		if (!strstrSearchTag(song, items[i].tagType,
				     items[i].needle)) {
			return 0;
		}
	}

	return 1;
}

static int tagItemFoundAndMatches(Song * song, int type, char *str)
{
	int i;

	if (type == LOCATE_TAG_FILE_TYPE || type == LOCATE_TAG_ANY_TYPE) {
		if (0 == strcmp(str, getSongUrl(song)))
			return 1;
		if (type == LOCATE_TAG_FILE_TYPE)
			return 0;
	}

	if (!song->tag)
		return 0;

	for (i = 0; i < song->tag->numOfItems; i++) {
		if (type != LOCATE_TAG_ANY_TYPE &&
		    song->tag->items[i].type != type) {
			continue;
		}

		if (0 == strcmp(str, song->tag->items[i].value))
			return 1;
	}

	return 0;
}


int tagItemsFoundAndMatches(Song * song, int numItems, LocateTagItem * items)
{
	int i;

	for (i = 0; i < numItems; i++) {
		if (!tagItemFoundAndMatches(song, items[i].tagType,
					    items[i].needle)) {
			return 0;
		}
	}

	return 1;
}
