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
#include "TagBuilder.hxx"
#include "TagSettings.h"
#include "TagPool.hxx"
#include "TagString.hxx"
#include "Tag.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

void
TagBuilder::Clear()
{
	time = -1;
	has_playlist = false;

	tag_pool_lock.lock();
	for (auto i : items)
		tag_pool_put_item(i);
	tag_pool_lock.unlock();

	items.clear();
}

void
TagBuilder::Commit(Tag &tag)
{
	tag.Clear();

	tag.time = time;
	tag.has_playlist = has_playlist;

	/* move all TagItem pointers to the new Tag object without
	   touching the TagPool reference counters; the
	   vector::clear() call is important to detach them from this
	   object */
	const unsigned n_items = items.size();
	tag.num_items = n_items;
	tag.items = g_new(TagItem *, n_items);
	std::copy_n(items.begin(), n_items, tag.items);
	items.clear();

	/* now ensure that this object is fresh (will not delete any
	   items because we've already moved them out) */
	Clear();
}

Tag *
TagBuilder::Commit()
{
	Tag *tag = new Tag();
	Commit(*tag);
	return tag;
}

inline void
TagBuilder::AddItemInternal(TagType type, const char *value, size_t length)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(value != nullptr);
#endif
	assert(length > 0);

	char *p = FixTagString(value, length);
	if (p != nullptr) {
		value = p;
		length = strlen(value);
	}

	tag_pool_lock.lock();
	auto i = tag_pool_get_item(type, value, length);
	tag_pool_lock.unlock();

	g_free(p);

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
