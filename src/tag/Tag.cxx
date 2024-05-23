// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Tag.hxx"
#include "Pool.hxx"
#include "Builder.hxx"

#include <cassert>

void
Tag::Clear() noexcept
{
	duration = SignedSongTime::Negative();
	has_playlist = false;

	if (num_items > 0) {
		assert(items != nullptr);
		const std::scoped_lock protect{tag_pool_lock};
		for (unsigned i = 0; i < num_items; ++i)
			tag_pool_put_item(items[i]);
		num_items = 0;
	}

	delete[] items;
	items = nullptr;
}

Tag::Tag(const Tag &other) noexcept
	:duration(other.duration), has_playlist(other.has_playlist),
	 num_items(other.num_items)
{
	if (num_items > 0) {
		items = new TagItem *[num_items];

		const std::scoped_lock protect{tag_pool_lock};
		for (unsigned i = 0; i < num_items; i++)
			items[i] = tag_pool_dup_item(other.items[i]);
	}
}

Tag
Tag::Merge(const Tag &base, const Tag &add) noexcept
{
	TagBuilder builder(add);
	builder.Complement(base);
	return builder.Commit();
}

std::unique_ptr<Tag>
Tag::MergePtr(const Tag &base, const Tag &add) noexcept
{
	TagBuilder builder(add);
	builder.Complement(base);
	return builder.CommitNew();
}

std::unique_ptr<Tag>
Tag::Merge(std::unique_ptr<Tag> base, std::unique_ptr<Tag> add) noexcept
{
	if (add == nullptr)
		return base;

	if (base == nullptr)
		return add;

	return MergePtr(*base, *add);
}

std::unique_ptr<Tag>
Tag::Merge(const Tag *base, const Tag *add) noexcept
{
	if (base == nullptr && add == nullptr)
		/* no tag */
		return nullptr;

	if (base == nullptr)
		return std::make_unique<Tag>(*add);

	if (add == nullptr)
		return std::make_unique<Tag>(*base);

	return MergePtr(*base, *add);
}

const char *
Tag::GetValue(TagType type) const noexcept
{
	assert(type < TAG_NUM_OF_ITEM_TYPES);

	for (const auto &item : *this)
		if (item.type == type)
			return item.value;

	return nullptr;
}

bool
Tag::HasType(TagType type) const noexcept
{
	return GetValue(type) != nullptr;
}

static TagType
DecaySort(TagType type) noexcept
{
	switch (type) {
	case TAG_ARTIST_SORT:
		return TAG_ARTIST;

	case TAG_ALBUM_SORT:
		return TAG_ALBUM;

	case TAG_ALBUM_ARTIST_SORT:
		return TAG_ALBUM_ARTIST;

	default:
		return TAG_NUM_OF_ITEM_TYPES;
	}
}

static TagType
Fallback(TagType type) noexcept
{
	switch (type) {
	case TAG_ALBUM_ARTIST:
		return TAG_ARTIST;

	case TAG_MUSICBRAINZ_ALBUMARTISTID:
		return TAG_MUSICBRAINZ_ARTISTID;

	default:
		return TAG_NUM_OF_ITEM_TYPES;
	}
}

const char *
Tag::GetSortValue(TagType type) const noexcept
{
	const char *value = GetValue(type);
	if (value != nullptr)
		return value;

	/* try without *_SORT */
	const auto no_sort_type = DecaySort(type);
	if (no_sort_type != TAG_NUM_OF_ITEM_TYPES) {
		value = GetValue(no_sort_type);
		if (value != nullptr)
			return value;
	}

	/* fall back from TAG_ALBUM_ARTIST to TAG_ALBUM */

	type = Fallback(type);
	if (type != TAG_NUM_OF_ITEM_TYPES)
		return GetSortValue(type);

	if (no_sort_type != TAG_NUM_OF_ITEM_TYPES) {
		type = Fallback(no_sort_type);
		if (type != TAG_NUM_OF_ITEM_TYPES)
			return GetSortValue(type);
	}

	/* finally fall back to empty string */

	return "";
}
