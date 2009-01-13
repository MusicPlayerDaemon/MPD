/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
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

#include "tag.h"
#include "tag_internal.h"
#include "tag_pool.h"
#include "log.h"
#include "conf.h"
#include "song.h"

#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Maximum number of items managed in the bulk list; if it is
 * exceeded, we switch back to "normal" reallocation.
 */
#define BULK_MAX 64

static struct {
#ifndef NDEBUG
	int busy;
#endif
	struct tag_item *items[BULK_MAX];
} bulk;

const char *mpdTagItemKeys[TAG_NUM_OF_ITEM_TYPES] = {
	"Artist",
	"Album",
	"AlbumArtist",
	"Title",
	"Track",
	"Name",
	"Genre",
	"Date",
	"Composer",
	"Performer",
	"Comment",
	"Disc"
};

int8_t ignoreTagItems[TAG_NUM_OF_ITEM_TYPES];

static size_t items_size(const struct tag *tag)
{
	return (tag->numOfItems * sizeof(struct tag_item *));
}

void tag_lib_init(void)
{
	int quit = 0;
	char *temp;
	char *s;
	char *c;
	ConfigParam *param;
	int i;

	/* parse the "metadata_to_use" config parameter below */

	memset(ignoreTagItems, 0, TAG_NUM_OF_ITEM_TYPES);
	ignoreTagItems[TAG_ITEM_COMMENT] = 1;	/* ignore comments by default */

	param = getConfigParam(CONF_METADATA_TO_USE);

	if (!param)
		return;

	memset(ignoreTagItems, 1, TAG_NUM_OF_ITEM_TYPES);

	if (0 == strcasecmp(param->value, "none"))
		return;

	temp = c = s = g_strdup(param->value);
	while (!quit) {
		if (*s == ',' || *s == '\0') {
			if (*s == '\0')
				quit = 1;
			*s = '\0';
			for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
				if (strcasecmp(c, mpdTagItemKeys[i]) == 0) {
					ignoreTagItems[i] = 0;
					break;
				}
			}
			if (strlen(c) && i == TAG_NUM_OF_ITEM_TYPES) {
				FATAL("error parsing metadata item \"%s\" at "
				      "line %i\n", c, param->line);
			}
			s++;
			c = s;
		}
		s++;
	}

	free(temp);
}

struct tag *tag_ape_load(const char *file)
{
	struct tag *ret = NULL;
	FILE *fp;
	int tagCount;
	char *buffer = NULL;
	char *p;
	size_t tagLen;
	size_t size;
	unsigned long flags;
	int i;
	char *key;

	struct {
		unsigned char id[8];
		uint32_t version;
		uint32_t length;
		uint32_t tagCount;
		unsigned char flags[4];
		unsigned char reserved[8];
	} footer;

	const char *apeItems[7] = {
		"title",
		"artist",
		"album",
		"comment",
		"genre",
		"track",
		"year"
	};

	int tagItems[7] = {
		TAG_ITEM_TITLE,
		TAG_ITEM_ARTIST,
		TAG_ITEM_ALBUM,
		TAG_ITEM_COMMENT,
		TAG_ITEM_GENRE,
		TAG_ITEM_TRACK,
		TAG_ITEM_DATE,
	};

	fp = fopen(file, "r");
	if (!fp)
		return NULL;

	/* determine if file has an apeV2 tag */
	if (fseek(fp, 0, SEEK_END))
		goto fail;
	size = (size_t)ftell(fp);
	if (fseek(fp, size - sizeof(footer), SEEK_SET))
		goto fail;
	if (fread(&footer, 1, sizeof(footer), fp) != sizeof(footer))
		goto fail;
	if (memcmp(footer.id, "APETAGEX", sizeof(footer.id)) != 0)
		goto fail;
	if (GUINT32_FROM_LE(footer.version) != 2000)
		goto fail;

	/* find beginning of ape tag */
	tagLen = GUINT32_FROM_LE(footer.length);
	if (tagLen < sizeof(footer))
		goto fail;
	if (fseek(fp, size - tagLen, SEEK_SET))
		goto fail;

	/* read tag into buffer */
	tagLen -= sizeof(footer);
	if (tagLen <= 0)
		goto fail;
	buffer = g_malloc(tagLen);
	if (fread(buffer, 1, tagLen, fp) != tagLen)
		goto fail;

	/* read tags */
	tagCount = GUINT32_FROM_LE(footer.tagCount);
	p = buffer;
	while (tagCount-- && tagLen > 10) {
		size = GUINT32_FROM_LE(*(const uint32_t *)p);
		p += 4;
		tagLen -= 4;
		flags = GUINT32_FROM_LE(*(const uint32_t *)p);
		p += 4;
		tagLen -= 4;

		/* get the key */
		key = p;
		while (tagLen - size > 0 && *p != '\0') {
			p++;
			tagLen--;
		}
		p++;
		tagLen--;

		/* get the value */
		if (tagLen < size)
			goto fail;

		/* we only care about utf-8 text tags */
		if (!(flags & (0x3 << 1))) {
			for (i = 0; i < 7; i++) {
				if (strcasecmp(key, apeItems[i]) == 0) {
					if (!ret)
						ret = tag_new();
					tag_add_item_n(ret, tagItems[i],
						       p, size);
				}
			}
		}
		p += size;
		tagLen -= size;
	}

fail:
	if (fp)
		fclose(fp);
	if (buffer)
		free(buffer);
	return ret;
}

struct tag *tag_new(void)
{
	struct tag *ret = g_new(struct tag, 1);
	ret->items = NULL;
	ret->time = -1;
	ret->numOfItems = 0;
	return ret;
}

static void deleteItem(struct tag *tag, int idx)
{
	assert(idx < tag->numOfItems);
	tag->numOfItems--;

	g_mutex_lock(tag_pool_lock);
	tag_pool_put_item(tag->items[idx]);
	g_mutex_unlock(tag_pool_lock);

	if (tag->numOfItems - idx > 0) {
		memmove(tag->items + idx, tag->items + idx + 1,
			tag->numOfItems - idx);
	}

	if (tag->numOfItems > 0) {
		tag->items = g_realloc(tag->items, items_size(tag));
	} else {
		free(tag->items);
		tag->items = NULL;
	}
}

void tag_clear_items_by_type(struct tag *tag, enum tag_type type)
{
	int i;

	for (i = 0; i < tag->numOfItems; i++) {
		if (tag->items[i]->type == type) {
			deleteItem(tag, i);
			/* decrement since when just deleted this node */
			i--;
		}
	}
}

void tag_free(struct tag *tag)
{
	int i;

	g_mutex_lock(tag_pool_lock);
	for (i = tag->numOfItems; --i >= 0; )
		tag_pool_put_item(tag->items[i]);
	g_mutex_unlock(tag_pool_lock);

	if (tag->items == bulk.items) {
#ifndef NDEBUG
		assert(bulk.busy);
		bulk.busy = 0;
#endif
	} else if (tag->items) {
		free(tag->items);
	}

	free(tag);
}

struct tag *tag_dup(const struct tag *tag)
{
	struct tag *ret;
	int i;

	if (!tag)
		return NULL;

	ret = tag_new();
	ret->time = tag->time;
	ret->numOfItems = tag->numOfItems;
	ret->items = ret->numOfItems > 0 ? g_malloc(items_size(tag)) : NULL;

	g_mutex_lock(tag_pool_lock);
	for (i = 0; i < tag->numOfItems; i++)
		ret->items[i] = tag_pool_dup_item(tag->items[i]);
	g_mutex_unlock(tag_pool_lock);

	return ret;
}

struct tag *
tag_merge(const struct tag *base, const struct tag *add)
{
	struct tag *ret;
	unsigned n;

	assert(base != NULL);
	assert(add != NULL);

	/* allocate new tag object */

	ret = tag_new();
	ret->time = add->time > 0 ? add->time : base->time;
	ret->numOfItems = base->numOfItems + add->numOfItems;
	ret->items = ret->numOfItems > 0 ? g_malloc(items_size(ret)) : NULL;

	g_mutex_lock(tag_pool_lock);

	/* copy all items from "add" */

	for (unsigned i = 0; i < add->numOfItems; ++i)
		ret->items[i] = tag_pool_dup_item(add->items[i]);

	n = add->numOfItems;

	/* copy additional items from "base" */

	for (unsigned i = 0; i < base->numOfItems; ++i)
		if (!tag_has_type(add, base->items[i]->type))
			ret->items[n++] = tag_pool_dup_item(base->items[i]);

	g_mutex_unlock(tag_pool_lock);

	assert(n <= ret->numOfItems);

	if (n < ret->numOfItems) {
		/* some tags were not copied - shrink ret->items */
		assert(n > 0);

		ret->numOfItems = n;
		ret->items = g_realloc(ret->items, items_size(ret));
	}

	return ret;
}

bool tag_has_type(const struct tag *tag, enum tag_type type)
{
	assert(tag != NULL);
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	for (unsigned i = 0; i < tag->numOfItems; i++)
		if (tag->items[i]->type == type)
			return true;

	return false;
}

int tag_equal(const struct tag *tag1, const struct tag *tag2)
{
	int i;

	if (tag1 == NULL && tag2 == NULL)
		return 1;
	else if (!tag1 || !tag2)
		return 0;

	if (tag1->time != tag2->time)
		return 0;

	if (tag1->numOfItems != tag2->numOfItems)
		return 0;

	for (i = 0; i < tag1->numOfItems; i++) {
		if (tag1->items[i]->type != tag2->items[i]->type)
			return 0;
		if (strcmp(tag1->items[i]->value, tag2->items[i]->value)) {
			return 0;
		}
	}

	return 1;
}

static char *
fix_utf8(const char *str, size_t length)
{
	char *temp;
	gsize written;

	assert(str != NULL);

	if (g_utf8_validate(str, length, NULL))
		return NULL;

	DEBUG("not valid utf8 in tag: %s\n",str);
	temp = g_convert(str, length, "utf-8", "iso-8859-1",
			 NULL, &written, NULL);
	if (temp == NULL)
		return NULL;

	return temp;
}

void tag_begin_add(struct tag *tag)
{
	assert(!bulk.busy);
	assert(tag != NULL);
	assert(tag->items == NULL);
	assert(tag->numOfItems == 0);

#ifndef NDEBUG
	bulk.busy = 1;
#endif
	tag->items = bulk.items;
}

void tag_end_add(struct tag *tag)
{
	if (tag->items == bulk.items) {
		assert(tag->numOfItems <= BULK_MAX);

		if (tag->numOfItems > 0) {
			/* copy the tag items from the bulk list over
			   to a new list (which fits exactly) */
			tag->items = g_malloc(items_size(tag));
			memcpy(tag->items, bulk.items, items_size(tag));
		} else
			tag->items = NULL;
	}

#ifndef NDEBUG
	bulk.busy = 0;
#endif
}

static bool
char_is_non_printable(unsigned char ch)
{
	return ch < 0x20;
}

static const char *
find_non_printable(const char *p, size_t length)
{
	for (size_t i = 0; i < length; ++i)
		if (char_is_non_printable(p[i]))
			return p + i;

	return NULL;
}

/**
 * Clears all non-printable characters, convert them to space.
 * Returns NULL if nothing needs to be cleared.
 */
static char *
clear_non_printable(const char *p, size_t length)
{
	const char *first = find_non_printable(p, length);
	char *dest;

	if (first == NULL)
		return NULL;

	dest = g_strndup(p, length);

	for (size_t i = first - p; i < length; ++i)
		if (char_is_non_printable(dest[i]))
			dest[i] = ' ';

	return dest;
}

static char *
fix_tag_value(const char *p, size_t length)
{
	char *utf8, *cleared;

	utf8 = fix_utf8(p, length);
	if (utf8 != NULL) {
		p = utf8;
		length = strlen(p);
	}

	cleared = clear_non_printable(p, length);
	if (cleared == NULL)
		cleared = utf8;
	else
		g_free(utf8);

	return cleared;
}

static void appendToTagItems(struct tag *tag, enum tag_type type,
			     const char *value, size_t len)
{
	unsigned int i = tag->numOfItems;
	char *p;

	p = fix_tag_value(value, len);
	if (p != NULL) {
		value = p;
		len = strlen(value);
	}

	tag->numOfItems++;

	if (tag->items != bulk.items)
		/* bulk mode disabled */
		tag->items = g_realloc(tag->items, items_size(tag));
	else if (tag->numOfItems >= BULK_MAX) {
		/* bulk list already full - switch back to non-bulk */
		assert(bulk.busy);

		tag->items = g_malloc(items_size(tag));
		memcpy(tag->items, bulk.items,
		       items_size(tag) - sizeof(struct tag_item *));
	}

	g_mutex_lock(tag_pool_lock);
	tag->items[i] = tag_pool_get_item(type, value, len);
	g_mutex_unlock(tag_pool_lock);

	g_free(p);
}

void tag_add_item_n(struct tag *tag, enum tag_type itemType,
		    const char *value, size_t len)
{
	if (ignoreTagItems[itemType])
	{
		return;
	}
	if (!value || !len)
		return;

	/* we can't hold more than 255 items */
	if (tag->numOfItems == 255)
		return;

	appendToTagItems(tag, itemType, value, len);
}
