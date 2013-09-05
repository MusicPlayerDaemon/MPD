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
#include "TagPool.hxx"
#include "TagString.hxx"
#include "TagSettings.h"

#include <glib.h>
#include <assert.h>
#include <string.h>

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

void
Tag::Clear()
{
	time = -1;
	has_playlist = false;

	tag_pool_lock.lock();
	for (unsigned i = 0; i < num_items; ++i)
		tag_pool_put_item(items[i]);
	tag_pool_lock.unlock();

	g_free(items);
	items = nullptr;
	num_items = 0;
}

Tag::~Tag()
{
	tag_pool_lock.lock();
	for (int i = num_items; --i >= 0; )
		tag_pool_put_item(items[i]);
	tag_pool_lock.unlock();

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

void
Tag::AddItemInternal(tag_type type, const char *value, size_t len)
{
	unsigned int i = num_items;

	char *p = FixTagString(value, len);
	if (p != nullptr) {
		value = p;
		len = strlen(value);
	}

	num_items++;

	items = (TagItem **)g_realloc(items, items_size(*this));

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
