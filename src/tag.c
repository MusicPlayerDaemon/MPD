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
#include "tag.h"
#include "tag_internal.h"
#include "tag_pool.h"
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
	bool busy;
#endif
	struct tag_item *items[BULK_MAX];
} bulk;

const char *tag_item_names[TAG_NUM_OF_ITEM_TYPES] = {
	[TAG_ARTIST] = "Artist",
	[TAG_ARTIST_SORT] = "ArtistSort",
	[TAG_ALBUM] = "Album",
	[TAG_ALBUM_ARTIST] = "AlbumArtist",
	[TAG_ALBUM_ARTIST_SORT] = "AlbumArtistSort",
	[TAG_TITLE] = "Title",
	[TAG_TRACK] = "Track",
	[TAG_NAME] = "Name",
	[TAG_GENRE] = "Genre",
	[TAG_DATE] = "Date",
	[TAG_COMPOSER] = "Composer",
	[TAG_PERFORMER] = "Performer",
	[TAG_COMMENT] = "Comment",
	[TAG_DISC] = "Disc",

	/* MusicBrainz tags from http://musicbrainz.org/doc/MusicBrainzTag */
	[TAG_MUSICBRAINZ_ARTISTID] = "MUSICBRAINZ_ARTISTID",
	[TAG_MUSICBRAINZ_ALBUMID] = "MUSICBRAINZ_ALBUMID",
	[TAG_MUSICBRAINZ_ALBUMARTISTID] = "MUSICBRAINZ_ALBUMARTISTID",
	[TAG_MUSICBRAINZ_TRACKID] = "MUSICBRAINZ_TRACKID",
};

bool ignore_tag_items[TAG_NUM_OF_ITEM_TYPES];

enum tag_type
tag_name_parse(const char *name)
{
	assert(name != NULL);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != NULL);

		if (strcmp(name, tag_item_names[i]) == 0)
			return (enum tag_type)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

enum tag_type
tag_name_parse_i(const char *name)
{
	assert(name != NULL);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != NULL);

		if (g_ascii_strcasecmp(name, tag_item_names[i]) == 0)
			return (enum tag_type)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

static size_t items_size(const struct tag *tag)
{
	return tag->num_items * sizeof(struct tag_item *);
}

void tag_lib_init(void)
{
	const char *value;
	int quit = 0;
	char *temp;
	char *s;
	char *c;
	enum tag_type type;

	/* parse the "metadata_to_use" config parameter below */

	/* ignore comments by default */
	ignore_tag_items[TAG_COMMENT] = true;

	value = config_get_string(CONF_METADATA_TO_USE, NULL);
	if (value == NULL)
		return;

	memset(ignore_tag_items, true, TAG_NUM_OF_ITEM_TYPES);

	if (0 == g_ascii_strcasecmp(value, "none"))
		return;

	temp = c = s = g_strdup(value);
	while (!quit) {
		if (*s == ',' || *s == '\0') {
			if (*s == '\0')
				quit = 1;
			*s = '\0';

			c = g_strstrip(c);
			if (*c == 0)
				continue;

			type = tag_name_parse_i(c);
			if (type == TAG_NUM_OF_ITEM_TYPES)
				g_error("error parsing metadata item \"%s\"",
					c);

			ignore_tag_items[type] = false;

			s++;
			c = s;
		}
		s++;
	}

	g_free(temp);
}

struct tag *tag_new(void)
{
	struct tag *ret = g_new(struct tag, 1);
	ret->items = NULL;
	ret->time = -1;
	ret->num_items = 0;
	return ret;
}

static void tag_delete_item(struct tag *tag, unsigned idx)
{
	assert(idx < tag->num_items);
	tag->num_items--;

	g_mutex_lock(tag_pool_lock);
	tag_pool_put_item(tag->items[idx]);
	g_mutex_unlock(tag_pool_lock);

	if (tag->num_items - idx > 0) {
		memmove(tag->items + idx, tag->items + idx + 1,
			tag->num_items - idx);
	}

	if (tag->num_items > 0) {
		tag->items = g_realloc(tag->items, items_size(tag));
	} else {
		g_free(tag->items);
		tag->items = NULL;
	}
}

void tag_clear_items_by_type(struct tag *tag, enum tag_type type)
{
	for (unsigned i = 0; i < tag->num_items; i++) {
		if (tag->items[i]->type == type) {
			tag_delete_item(tag, i);
			/* decrement since when just deleted this node */
			i--;
		}
	}
}

void tag_free(struct tag *tag)
{
	int i;

	assert(tag != NULL);

	g_mutex_lock(tag_pool_lock);
	for (i = tag->num_items; --i >= 0; )
		tag_pool_put_item(tag->items[i]);
	g_mutex_unlock(tag_pool_lock);

	if (tag->items == bulk.items) {
#ifndef NDEBUG
		assert(bulk.busy);
		bulk.busy = false;
#endif
	} else
		g_free(tag->items);

	g_free(tag);
}

struct tag *tag_dup(const struct tag *tag)
{
	struct tag *ret;

	if (!tag)
		return NULL;

	ret = tag_new();
	ret->time = tag->time;
	ret->num_items = tag->num_items;
	ret->items = ret->num_items > 0 ? g_malloc(items_size(tag)) : NULL;

	g_mutex_lock(tag_pool_lock);
	for (unsigned i = 0; i < tag->num_items; i++)
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
	ret->num_items = base->num_items + add->num_items;
	ret->items = ret->num_items > 0 ? g_malloc(items_size(ret)) : NULL;

	g_mutex_lock(tag_pool_lock);

	/* copy all items from "add" */

	for (unsigned i = 0; i < add->num_items; ++i)
		ret->items[i] = tag_pool_dup_item(add->items[i]);

	n = add->num_items;

	/* copy additional items from "base" */

	for (unsigned i = 0; i < base->num_items; ++i)
		if (!tag_has_type(add, base->items[i]->type))
			ret->items[n++] = tag_pool_dup_item(base->items[i]);

	g_mutex_unlock(tag_pool_lock);

	assert(n <= ret->num_items);

	if (n < ret->num_items) {
		/* some tags were not copied - shrink ret->items */
		assert(n > 0);

		ret->num_items = n;
		ret->items = g_realloc(ret->items, items_size(ret));
	}

	return ret;
}

const char *
tag_get_value(const struct tag *tag, enum tag_type type)
{
	assert(tag != NULL);
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	for (unsigned i = 0; i < tag->num_items; i++)
		if (tag->items[i]->type == type)
			return tag->items[i]->value;

	return NULL;
}

bool tag_has_type(const struct tag *tag, enum tag_type type)
{
	return tag_get_value(tag, type) != NULL;
}

bool tag_equal(const struct tag *tag1, const struct tag *tag2)
{
	if (tag1 == NULL && tag2 == NULL)
		return true;
	else if (!tag1 || !tag2)
		return false;

	if (tag1->time != tag2->time)
		return false;

	if (tag1->num_items != tag2->num_items)
		return false;

	for (unsigned i = 0; i < tag1->num_items; i++) {
		if (tag1->items[i]->type != tag2->items[i]->type)
			return false;
		if (strcmp(tag1->items[i]->value, tag2->items[i]->value)) {
			return false;
		}
	}

	return true;
}

/**
 * Replace invalid sequences with the question mark.
 */
static char *
patch_utf8(const char *src, size_t length, const gchar *end)
{
	/* duplicate the string, and replace invalid bytes in that
	   buffer */
	char *dest = g_strdup(src);

	do {
		dest[end - src] = '?';
	} while (!g_utf8_validate(end + 1, (src + length) - (end + 1), &end));

	return dest;
}

static char *
fix_utf8(const char *str, size_t length)
{
	const gchar *end;
	char *temp;
	gsize written;

	assert(str != NULL);

	/* check if the string is already valid UTF-8 */
	if (g_utf8_validate(str, length, &end))
		return NULL;

	/* no, it's not - try to import it from ISO-Latin-1 */
	temp = g_convert(str, length, "utf-8", "iso-8859-1",
			 NULL, &written, NULL);
	if (temp != NULL)
		/* success! */
		return temp;

	/* no, still broken - there's no medication, just patch
	   invalid sequences */
	return patch_utf8(str, length, end);
}

void tag_begin_add(struct tag *tag)
{
	assert(!bulk.busy);
	assert(tag != NULL);
	assert(tag->items == NULL);
	assert(tag->num_items == 0);

#ifndef NDEBUG
	bulk.busy = true;
#endif
	tag->items = bulk.items;
}

void tag_end_add(struct tag *tag)
{
	if (tag->items == bulk.items) {
		assert(tag->num_items <= BULK_MAX);

		if (tag->num_items > 0) {
			/* copy the tag items from the bulk list over
			   to a new list (which fits exactly) */
			tag->items = g_malloc(items_size(tag));
			memcpy(tag->items, bulk.items, items_size(tag));
		} else
			tag->items = NULL;
	}

#ifndef NDEBUG
	bulk.busy = false;
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

static void
tag_add_item_internal(struct tag *tag, enum tag_type type,
		      const char *value, size_t len)
{
	unsigned int i = tag->num_items;
	char *p;

	p = fix_tag_value(value, len);
	if (p != NULL) {
		value = p;
		len = strlen(value);
	}

	tag->num_items++;

	if (tag->items != bulk.items)
		/* bulk mode disabled */
		tag->items = g_realloc(tag->items, items_size(tag));
	else if (tag->num_items >= BULK_MAX) {
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

void tag_add_item_n(struct tag *tag, enum tag_type type,
		    const char *value, size_t len)
{
	if (ignore_tag_items[type])
	{
		return;
	}
	if (!value || !len)
		return;

	tag_add_item_internal(tag, type, value, len);
}
