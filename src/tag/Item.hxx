// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ITEM_HXX
#define MPD_TAG_ITEM_HXX

#include <cstdint>
#include <cstring>

enum TagType : uint8_t;

/**
 * One tag value.  It is a mapping of #TagType to am arbitrary string
 * value.  Each tag can have multiple items of one tag type (although
 * few clients support that).
 */
struct TagItem {
	/** the type of this item */
	TagType type;

	/**
	 * the value of this tag; this is a variable length string
	 */
	char value[1];

	bool operator==(const TagItem &other) const noexcept {
		return (this == &other) ? true :
			type == other.type && std::strcmp(value, other.value) == 0;
	}

private:
	/* making the constructor private
	   to only allow construction by TagPoolItem. */
	friend struct TagPoolItem;
	TagItem() = default;
	TagItem(const TagItem &other) = delete;
	TagItem &operator=(const TagItem &other) = delete;
};

static_assert(sizeof(TagItem) == 2, "Unexpected size");
static_assert(alignof(TagItem) == 1, "Unexpected alignment");

#endif
