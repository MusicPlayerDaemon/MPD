/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Tag.hxx"
#include "TagInternal.hxx"
#include "TagPool.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#include "Song.hxx"
#include "mpd_error.h"

#include <glib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Maximum number of items managed in the bulk list; if it is
 * exceeded, we switch back to "normal" reallocation.
 */
#define BULK_MAX 64

static struct {
#ifndef NDEBUG
	bool busy;
#endif
	TagItem *items[BULK_MAX];
} bulk;

bool ignore_tag_items[TAG_NUM_OF_ITEM_TYPES];

enum tag_type
tag_name_parse(const char *name)
{
	assert(name != nullptr);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (strcmp(name, tag_item_names[i]) == 0)
			return (enum tag_type)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

enum tag_type
tag_name_parse_i(const char *name)
{
	assert(name != nullptr);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (g_ascii_strcasecmp(name, tag_item_names[i]) == 0)
			return (enum tag_type)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

static size_t
items_size(const Tag &tag)
{
	return tag.num_items * sizeof(TagItem *);
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

	value = config_get_string(CONF_METADATA_TO_USE, nullptr);
	if (value == nullptr)
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
				MPD_ERROR("error parsing metadata item \"%s\"",
					  c);

			ignore_tag_items[type] = false;

			s++;
			c = s;
		}
		s++;
	}

	g_free(temp);
}

void
Tag::Clear()
{
	time = -1;
	has_playlist = false;

	tag_pool_lock.lock();
	for (unsigned i = 0; i < num_items; ++i)
		tag_pool_put_item(items[i]);
	tag_pool_lock.unlock();

	if (items == bulk.items) {
#ifndef NDEBUG
		assert(bulk.busy);
		bulk.busy = false;
#endif
	} else
		g_free(items);

	items = nullptr;
	num_items = 0;
}

void
Tag::DeleteItem(unsigned idx)
{
	assert(idx < num_items);
	--num_items;

	tag_pool_lock.lock();
	tag_pool_put_item(items[idx]);
	tag_pool_lock.unlock();

	if (num_items - idx > 0) {
		memmove(items + idx, items + idx + 1,
			(num_items - idx) * sizeof(items[0]));
	}

	if (num_items > 0) {
		items = (TagItem **)
			g_realloc(items, items_size(*this));
	} else {
		g_free(items);
		items = nullptr;
	}
}

void
Tag::ClearItemsByType(tag_type type)
{
	for (unsigned i = 0; i < num_items; i++) {
		if (items[i]->type == type) {
			DeleteItem(i);
			/* decrement since when just deleted this node */
			i--;
		}
	}
}

Tag::~Tag()
{
	tag_pool_lock.lock();
	for (int i = num_items; --i >= 0; )
		tag_pool_put_item(items[i]);
	tag_pool_lock.unlock();

	if (items == bulk.items) {
#ifndef NDEBUG
		assert(bulk.busy);
		bulk.busy = false;
#endif
	} else
		g_free(items);
}

Tag::Tag(const Tag &other)
	:time(other.time), has_playlist(other.has_playlist),
	 items(nullptr),
	 num_items(other.num_items)
{
	if (num_items > 0) {
		items = (TagItem **)g_malloc(items_size(other));

		tag_pool_lock.lock();
		for (unsigned i = 0; i < num_items; i++)
			items[i] = tag_pool_dup_item(other.items[i]);
		tag_pool_lock.unlock();
	}
}

Tag *
Tag::Merge(const Tag &base, const Tag &add)
{
	unsigned n;

	/* allocate new tag object */

	Tag *ret = new Tag();
	ret->time = add.time > 0 ? add.time : base.time;
	ret->num_items = base.num_items + add.num_items;
	ret->items = ret->num_items > 0
		? (TagItem **)g_malloc(items_size(*ret))
		: nullptr;

	tag_pool_lock.lock();

	/* copy all items from "add" */

	for (unsigned i = 0; i < add.num_items; ++i)
		ret->items[i] = tag_pool_dup_item(add.items[i]);

	n = add.num_items;

	/* copy additional items from "base" */

	for (unsigned i = 0; i < base.num_items; ++i)
		if (!add.HasType(base.items[i]->type))
			ret->items[n++] = tag_pool_dup_item(base.items[i]);

	tag_pool_lock.unlock();

	assert(n <= ret->num_items);

	if (n < ret->num_items) {
		/* some tags were not copied - shrink ret->items */
		assert(n > 0);

		ret->num_items = n;
		ret->items = (TagItem **)
			g_realloc(ret->items, items_size(*ret));
	}

	return ret;
}

Tag *
Tag::MergeReplace(Tag *base, Tag *add)
{
	if (add == nullptr)
		return base;

	if (base == nullptr)
		return add;

	Tag *tag = Merge(*base, *add);
	delete base;
	delete add;

	return tag;
}

const char *
Tag::GetValue(tag_type type) const
{
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	for (unsigned i = 0; i < num_items; i++)
		if (items[i]->type == type)
			return items[i]->value;

	return nullptr;
}

bool
Tag::HasType(tag_type type) const
{
	return GetValue(type) != nullptr;
}

bool
Tag::Equals(const Tag &other) const
{
	if (time != other.time)
		return false;

	if (num_items != other.num_items)
		return false;

	for (unsigned i = 0; i < num_items; i++) {
		if (items[i]->type != other.items[i]->type)
			return false;
		if (strcmp(items[i]->value, other.items[i]->value)) {
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

	assert(str != nullptr);

	/* check if the string is already valid UTF-8 */
	if (g_utf8_validate(str, length, &end))
		return nullptr;

	/* no, it's not - try to import it from ISO-Latin-1 */
	temp = g_convert(str, length, "utf-8", "iso-8859-1",
			 nullptr, &written, nullptr);
	if (temp != nullptr)
		/* success! */
		return temp;

	/* no, still broken - there's no medication, just patch
	   invalid sequences */
	return patch_utf8(str, length, end);
}

void
Tag::BeginAdd()
{
	assert(!bulk.busy);
	assert(items == nullptr);
	assert(num_items == 0);

#ifndef NDEBUG
	bulk.busy = true;
#endif
	items = bulk.items;
}

void
Tag::EndAdd()
{
	if (items == bulk.items) {
		assert(num_items <= BULK_MAX);

		if (num_items > 0) {
			/* copy the tag items from the bulk list over
			   to a new list (which fits exactly) */
			items = (TagItem **)
				g_malloc(items_size(*this));
			memcpy(items, bulk.items, items_size(*this));
		} else
			items = nullptr;
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

	return nullptr;
}

/**
 * Clears all non-printable characters, convert them to space.
 * Returns nullptr if nothing needs to be cleared.
 */
static char *
clear_non_printable(const char *p, size_t length)
{
	const char *first = find_non_printable(p, length);
	char *dest;

	if (first == nullptr)
		return nullptr;

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
	if (utf8 != nullptr) {
		p = utf8;
		length = strlen(p);
	}

	cleared = clear_non_printable(p, length);
	if (cleared == nullptr)
		cleared = utf8;
	else
		g_free(utf8);

	return cleared;
}

void
Tag::AddItemInternal(tag_type type, const char *value, size_t len)
{
	unsigned int i = num_items;
	char *p;

	p = fix_tag_value(value, len);
	if (p != nullptr) {
		value = p;
		len = strlen(value);
	}

	num_items++;

	if (items != bulk.items)
		/* bulk mode disabled */
		items = (TagItem **)
			g_realloc(items, items_size(*this));
	else if (num_items >= BULK_MAX) {
		/* bulk list already full - switch back to non-bulk */
		assert(bulk.busy);

		items = (TagItem **)g_malloc(items_size(*this));
		memcpy(items, bulk.items,
		       items_size(*this) - sizeof(TagItem *));
	}

	tag_pool_lock.lock();
	items[i] = tag_pool_get_item(type, value, len);
	tag_pool_lock.unlock();

	g_free(p);
}

void
Tag::AddItem(tag_type type, const char *value, size_t len)
{
	if (ignore_tag_items[type])
		return;

	if (value == nullptr || len == 0)
		return;

	AddItemInternal(type, value, len);
}

void
Tag::AddItem(tag_type type, const char *value)
{
	AddItem(type, value, strlen(value));
}
