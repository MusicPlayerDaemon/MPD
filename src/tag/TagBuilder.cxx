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
#include "TagBuilder.hxx"
#include "TagSettings.h"
#include "TagPool.hxx"
#include "TagString.hxx"
#include "Tag.hxx"
#include "util/WritableBuffer.hxx"

#include <array>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

TagBuilder::TagBuilder(const Tag &other)
	:duration(other.duration), has_playlist(other.has_playlist)
{
	items.reserve(other.num_items);

	tag_pool_lock.lock();
	for (unsigned i = 0, n = other.num_items; i != n; ++i)
		items.push_back(tag_pool_dup_item(other.items[i]));
	tag_pool_lock.unlock();
}

TagBuilder::TagBuilder(Tag &&other)
	:duration(other.duration), has_playlist(other.has_playlist)
{
	/* move all TagItem pointers from the Tag object; we don't
	   need to contact the tag pool, because all we do is move
	   references */
	items.reserve(other.num_items);
	std::copy_n(other.items, other.num_items, std::back_inserter(items));

	/* discard the pointers from the Tag object */
	other.num_items = 0;
	delete[] other.items;
	other.items = nullptr;
}

TagBuilder &
TagBuilder::operator=(const TagBuilder &other)
{
	/* copy all attributes */
	duration = other.duration;
	has_playlist = other.has_playlist;
	items = other.items;

	/* increment the tag pool refcounters */
	tag_pool_lock.lock();
	for (auto i : items)
		tag_pool_dup_item(i);
	tag_pool_lock.unlock();

	return *this;
}

TagBuilder &
TagBuilder::operator=(TagBuilder &&other)
{
	duration = other.duration;
	has_playlist = other.has_playlist;
	items = std::move(other.items);

	return *this;
}

TagBuilder &
TagBuilder::operator=(Tag &&other)
{
	duration = other.duration;
	has_playlist = other.has_playlist;

	/* move all TagItem pointers from the Tag object; we don't
	   need to contact the tag pool, because all we do is move
	   references */
	items.clear();
	items.reserve(other.num_items);
	std::copy_n(other.items, other.num_items, std::back_inserter(items));

	/* discard the pointers from the Tag object */
	other.num_items = 0;
	delete[] other.items;
	other.items = nullptr;

	return *this;
}

void
TagBuilder::Clear()
{
	duration = SignedSongTime::Negative();
	has_playlist = false;
	RemoveAll();
}

void
TagBuilder::Commit(Tag &tag)
{
	tag.Clear();

	tag.duration = duration;
	tag.has_playlist = has_playlist;

	/* move all TagItem pointers to the new Tag object without
	   touching the TagPool reference counters; the
	   vector::clear() call is important to detach them from this
	   object */
	const unsigned n_items = items.size();
	tag.num_items = n_items;
	tag.items = new TagItem *[n_items];
	std::copy_n(items.begin(), n_items, tag.items);
	items.clear();

	/* now ensure that this object is fresh (will not delete any
	   items because we've already moved them out) */
	Clear();
}

Tag
TagBuilder::Commit()
{
	Tag tag;
	Commit(tag);
	return tag;
}

Tag *
TagBuilder::CommitNew()
{
	Tag *tag = new Tag();
	Commit(*tag);
	return tag;
}

bool
TagBuilder::HasType(TagType type) const
{
	for (auto i : items)
		if (i->type == type)
			return true;

	return false;
}

void
TagBuilder::Complement(const Tag &other)
{
	if (duration.IsNegative())
		duration = other.duration;

	has_playlist |= other.has_playlist;

	/* build a table of tag types that were already present in
	   this object, which will not be copied from #other */
	std::array<bool, TAG_NUM_OF_ITEM_TYPES> present;
	present.fill(false);
	for (const TagItem *i : items)
		present[i->type] = true;

	items.reserve(items.size() + other.num_items);

	tag_pool_lock.lock();
	for (unsigned i = 0, n = other.num_items; i != n; ++i) {
		TagItem *item = other.items[i];
		if (!present[item->type])
			items.push_back(tag_pool_dup_item(item));
	}
	tag_pool_lock.unlock();
}

inline void
TagBuilder::AddItemInternal(TagType type, const char *value, size_t length)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(value != nullptr);
#endif
	assert(length > 0);

	auto f = FixTagString(value, length);
	if (!f.IsNull()) {
		value = f.data;
		length = f.size;
	}

	tag_pool_lock.lock();
	auto i = tag_pool_get_item(type, value, length);
	tag_pool_lock.unlock();

	free(f.data);

	items.push_back(i);
}

void
TagBuilder::AddItem(TagType type, const char *value, size_t length)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(value != nullptr);
#endif

	if (length == 0 || ignore_tag_items[type])
		return;

	AddItemInternal(type, value, length);
}

void
TagBuilder::AddItem(TagType type, const char *value)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(value != nullptr);
#endif

	AddItem(type, value, strlen(value));
}

void
TagBuilder::AddEmptyItem(TagType type)
{
	tag_pool_lock.lock();
	auto i = tag_pool_get_item(type, "", 0);
	tag_pool_lock.unlock();

	items.push_back(i);
}

void
TagBuilder::RemoveAll()
{
	tag_pool_lock.lock();
	for (auto i : items)
		tag_pool_put_item(i);
	tag_pool_lock.unlock();

	items.clear();
}

void
TagBuilder::RemoveType(TagType type)
{
	const auto begin = items.begin(), end = items.end();

	items.erase(std::remove_if(begin, end,
				   [type](TagItem *item) {
					   if (item->type != type)
						   return false;
					   tag_pool_put_item(item);
					   return true;
				   }),
		    end);
}
