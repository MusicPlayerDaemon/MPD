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
#include "Tag.hxx"
#include "TagPool.hxx"
#include "TagString.hxx"
#include "TagSettings.h"
#include "TagBuilder.hxx"
#include "util/ASCII.hxx"

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

void
Tag::Clear()
{
	duration = SignedSongTime::Negative();
	has_playlist = false;

	tag_pool_lock.lock();
	for (unsigned i = 0; i < num_items; ++i)
		tag_pool_put_item(items[i]);
	tag_pool_lock.unlock();

	delete[] items;
	items = nullptr;
	num_items = 0;
}

Tag::Tag(const Tag &other)
	:duration(other.duration), has_playlist(other.has_playlist),
	 num_items(other.num_items),
	 items(nullptr)
{
	if (num_items > 0) {
		items = new TagItem *[num_items];

		tag_pool_lock.lock();
		for (unsigned i = 0; i < num_items; i++)
			items[i] = tag_pool_dup_item(other.items[i]);
		tag_pool_lock.unlock();
	}
}

Tag *
Tag::Merge(const Tag &base, const Tag &add)
{
	TagBuilder builder(add);
	builder.Complement(base);
	return builder.CommitNew();
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

	for (const auto &item : *this)
		if (item.type == type)
			return item.value;

	return nullptr;
}

bool
Tag::HasType(TagType type) const
{
	return GetValue(type) != nullptr;
}
