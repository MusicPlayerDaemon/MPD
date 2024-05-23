// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Builder.hxx"
#include "Settings.hxx"
#include "Pool.hxx"
#include "FixString.hxx"
#include "Tag.hxx"
#include "util/AllocatedArray.hxx"

#include <algorithm>
#include <array>
#include <cassert>

#include <stdlib.h>

TagBuilder::TagBuilder(const Tag &other) noexcept
	:duration(other.duration), has_playlist(other.has_playlist)
{
	const std::size_t n = other.num_items;
	if (n > 0) {
		items.reserve(other.num_items);
		const std::scoped_lock protect{tag_pool_lock};
		for (std::size_t i = 0; i != n; ++i)
			items.push_back(tag_pool_dup_item(other.items[i]));
	}
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

	RemoveAll();

	if (!other.items.empty()) {
		items = other.items;

		/* increment the tag pool refcounters */
		const std::scoped_lock protect{tag_pool_lock};
		for (auto &i : items)
			i = tag_pool_dup_item(i);
	}

	return *this;
}

TagBuilder &
TagBuilder::operator=(TagBuilder &&other) noexcept
{
	using std::swap;

	duration = other.duration;
	has_playlist = other.has_playlist;

	/* swap the two TagItem lists so we don't need to touch the
	   tag pool just yet */
	swap(items, other.items);

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
	RemoveAll();
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
	auto tag = std::make_unique<Tag>();
	Commit(*tag);
	return tag;
}

bool
TagBuilder::HasType(TagType type) const noexcept
{
	return std::any_of(items.begin(), items.end(), [type](const auto &i) { return i->type == type; });
}

void
TagBuilder::Complement(const Tag &other) noexcept
{
	if (duration.IsNegative())
		duration = other.duration;

	has_playlist |= other.has_playlist;

	const std::size_t n = other.num_items;
	if (n > 0) {
		/* build a table of tag types that were already present in
		   this object, which will not be copied from #other */
		std::array<bool, TAG_NUM_OF_ITEM_TYPES> present;
		present.fill(false);
		for (const TagItem *i : items)
			present[i->type] = true;

		items.reserve(items.size() + n);

		const std::scoped_lock protect{tag_pool_lock};
		for (std::size_t i = 0; i != n; ++i) {
			TagItem *item = other.items[i];
			if (!present[item->type])
				items.push_back(tag_pool_dup_item(item));
		}
	}
}

void
TagBuilder::AddItemUnchecked(TagType type, std::string_view value) noexcept
{
	TagItem *i;

	{
		const std::scoped_lock protect{tag_pool_lock};
		i = tag_pool_get_item(type, value);
	}

	items.push_back(i);
}

inline void
TagBuilder::AddItemInternal(TagType type, std::string_view value) noexcept
{
	assert(!value.empty());

	auto f = FixTagString(value);
	if (f != nullptr)
		value = { f.data(), f.size() };

	AddItemUnchecked(type, value);
}

void
TagBuilder::AddItem(TagType type, std::string_view value) noexcept
{
	if (value.empty() || !IsTagEnabled(type))
		return;

	AddItemInternal(type, value);
}

void
TagBuilder::AddEmptyItem(TagType type) noexcept
{
	AddItemUnchecked(type, "");
}

void
TagBuilder::RemoveAll() noexcept
{
	if (items.empty())
		/* don't acquire the tag_pool_lock if we're not going
		   to call tag_pool_put_item() anyway */
		return;

	{
		const std::scoped_lock protect{tag_pool_lock};
		for (auto i : items)
			tag_pool_put_item(i);
	}

	items.clear();
}

void
TagBuilder::RemoveType(TagType type) noexcept
{
	if (items.empty())
		/* don't acquire the tag_pool_lock if we're not going
		   to call tag_pool_put_item() anyway */
		return;

	const auto begin = items.begin(), end = items.end();

	const std::scoped_lock protect{tag_pool_lock};
	items.erase(std::remove_if(begin, end,
				   [type](TagItem *item) {
					   if (item->type != type)
						   return false;
					   tag_pool_put_item(item);
					   return true;
				   }),
		    end);
}
