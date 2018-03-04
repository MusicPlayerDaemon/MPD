/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Builder.hxx"
#include "Settings.hxx"
#include "Pool.hxx"
#include "FixString.hxx"
#include "Tag.hxx"
#include "ParseName.hxx"
#include "util/WritableBuffer.hxx"
#include "util/StringView.hxx"
#include "util/ASCII.hxx"
#include "command/Request.hxx"

#include <array>

#include <assert.h>
#include <stdlib.h>

TagBuilder::TagBuilder(const Tag &other) noexcept
	:duration(other.duration), has_playlist(other.has_playlist)
{
	items.reserve(other.num_items);

	const std::lock_guard<Mutex> protect(tag_pool_lock);

	for (unsigned i = 0, n = other.num_items; i != n; ++i)
		items.push_back(tag_pool_dup_item(other.items[i]));
}

TagBuilder::TagBuilder(Tag &&other) noexcept
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
TagBuilder::operator=(const TagBuilder &other) noexcept
{
	/* copy all attributes */
	duration = other.duration;
	has_playlist = other.has_playlist;
	items = other.items;

	/* increment the tag pool refcounters */
	const std::lock_guard<Mutex> protect(tag_pool_lock);
	for (auto i : items)
		tag_pool_dup_item(i);

	return *this;
}

TagBuilder &
TagBuilder::operator=(TagBuilder &&other) noexcept
{
	duration = other.duration;
	has_playlist = other.has_playlist;
	items = std::move(other.items);

	return *this;
}

TagBuilder &
TagBuilder::operator=(Tag &&other) noexcept
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
TagBuilder::Clear() noexcept
{
	duration = SignedSongTime::Negative();
	has_playlist = false;
	RemoveAll();
}

bool
TagBuilder::Parse(Request args)
{
	if (args.size == 0 || args.size % 2 != 0)
		return false;

	for (unsigned i = 0; i < args.size; i += 2) {
		if (StringEqualsCaseASCII(args[i], "duration_ms")) {
			duration = SignedSongTime::FromMS(args.ParseUnsigned(i+1));
		} else if (StringEqualsCaseASCII(args[i], "duration")) {
			duration = SignedSongTime::FromS(args.ParseFloat(i+1));
		} else {
			const TagType tag_type = tag_name_parse_i(args[i]);
			if (tag_type == TAG_NUM_OF_ITEM_TYPES) {
				return false;
			}
			AddItem(tag_type, args[i + 1]);
		}
	}

	return true;
}

void
TagBuilder::Commit(Tag &tag) noexcept
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
TagBuilder::Commit() noexcept
{
	Tag tag;
	Commit(tag);
	return tag;
}

std::unique_ptr<Tag>
TagBuilder::CommitNew() noexcept
{
	std::unique_ptr<Tag> tag(new Tag());
	Commit(*tag);
	return tag;
}

bool
TagBuilder::HasType(TagType type) const noexcept
{
	for (auto i : items)
		if (i->type == type)
			return true;

	return false;
}

void
TagBuilder::Complement(const Tag &other) noexcept
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

	const std::lock_guard<Mutex> protect(tag_pool_lock);
	for (unsigned i = 0, n = other.num_items; i != n; ++i) {
		TagItem *item = other.items[i];
		if (!present[item->type])
			items.push_back(tag_pool_dup_item(item));
	}
}

inline void
TagBuilder::AddItemInternal(TagType type, StringView value) noexcept
{
	assert(!value.empty());

	auto f = FixTagString(value);
	if (!f.IsNull())
		value = { f.data, f.size };

	TagItem *i;
	{
		const std::lock_guard<Mutex> protect(tag_pool_lock);
		i = tag_pool_get_item(type, value);
	}

	free(f.data);

	items.push_back(i);
}

void
TagBuilder::AddItem(TagType type, StringView value) noexcept
{
	if (value.empty() || !IsTagEnabled(type))
		return;

	AddItemInternal(type, value);
}

void
TagBuilder::AddItem(TagType type, const char *value) noexcept
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(value != nullptr);
#endif

	AddItem(type, StringView(value));
}

void
TagBuilder::AddEmptyItem(TagType type) noexcept
{
	TagItem *i;
	{
		const std::lock_guard<Mutex> protect(tag_pool_lock);
		i = tag_pool_get_item(type, "");
	}

	items.push_back(i);
}

void
TagBuilder::RemoveAll() noexcept
{
	{
		const std::lock_guard<Mutex> protect(tag_pool_lock);
		for (auto i : items)
			tag_pool_put_item(i);
	}

	items.clear();
}

void
TagBuilder::RemoveType(TagType type) noexcept
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
