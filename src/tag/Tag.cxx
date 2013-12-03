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
#include "TagBuilder.hxx"
#include "util/ASCII.hxx"

#include <glib.h>
#include <assert.h>
#include <string.h>

TagType
tag_name_parse(const char *name)
{
	assert(name != nullptr);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (strcmp(name, tag_item_names[i]) == 0)
			return (TagType)i;
	}

	return TAG_NUM_OF_ITEM_TYPES;
}

TagType
tag_name_parse_i(const char *name)
{
	assert(name != nullptr);

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		assert(tag_item_names[i] != nullptr);

		if (StringEqualsCaseASCII(name, tag_item_names[i]))
			return (TagType)i;
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
	TagBuilder builder(base);
	builder.Complement(add);
	return builder.Commit();
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
Tag::GetValue(TagType type) const
{
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	for (unsigned i = 0; i < num_items; i++)
		if (items[i]->type == type)
			return items[i]->value;

	return nullptr;
}

bool
Tag::HasType(TagType type) const
{
	return GetValue(type) != nullptr;
}

void
Tag::AddItemInternal(TagType type, const char *value, size_t len)
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
Tag::AddItem(TagType type, const char *value, size_t len)
{
	if (ignore_tag_items[type])
		return;

	if (value == nullptr || len == 0)
		return;

	AddItemInternal(type, value, len);
}

void
Tag::AddItem(TagType type, const char *value)
{
	AddItem(type, value, strlen(value));
}

void
Tag::RemoveType(TagType type)
{
	auto dest = items, src = items, end = items + num_items;

	tag_pool_lock.lock();
	while (src != end) {
		TagItem *item = *src++;
		if (item->type == type)
			/* remove it */
			tag_pool_put_item(item);
		else
			/* keep it */
			*dest++ = item;
	}
	tag_pool_lock.unlock();

	num_items = dest - items;
}
