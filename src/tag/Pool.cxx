// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Pool.hxx"
#include "Item.hxx"
#include "util/Cast.hxx"
#include "util/IntrusiveList.hxx"
#include "util/VarSize.hxx"

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>

#include <string.h>
#include <stdlib.h>

Mutex tag_pool_lock;

struct TagPoolItem {
	IntrusiveListHook<IntrusiveHookMode::NORMAL> list_hook;
	uint8_t ref = 1;
	TagItem item;

	static constexpr unsigned MAX_REF = std::numeric_limits<decltype(ref)>::max();

	TagPoolItem(TagType type,
		    std::string_view value) noexcept {
		item.type = type;
		*std::copy(value.begin(), value.end(), item.value) = 0;
	}

	static TagPoolItem *Create(TagType type,
				   std::string_view value) noexcept;
};

TagPoolItem *
TagPoolItem::Create(TagType type,
		    std::string_view value) noexcept
{
	TagPoolItem *dummy;
	return NewVarSize<TagPoolItem>(sizeof(dummy->item.value),
				       value.size() + 1,
				       type,
				       value);
}

static std::array<IntrusiveList<TagPoolItem,
				IntrusiveListMemberHookTraits<&TagPoolItem::list_hook>,
				IntrusiveListOptions{.zero_initialized = true}>,
		  16127> slots;

static inline unsigned
calc_hash(TagType type, std::string_view p) noexcept
{
	unsigned hash = 5381;

	for (auto ch : p)
		hash = (hash << 5) + hash + ch;

	return hash ^ type;
}

static constexpr TagPoolItem *
TagItemToPoolItem(TagItem *item) noexcept
{
	return &ContainerCast(*item, &TagPoolItem::item);
}

static inline auto &
tag_value_list(TagType type, std::string_view value) noexcept
{
	return slots[calc_hash(type, value) % slots.size()];
}

TagItem *
tag_pool_get_item(TagType type, std::string_view value) noexcept
{
	auto &list = tag_value_list(type, value);

	for (auto &i : list) {
		if (i.item.type == type &&
		    value == i.item.value &&
		    i.ref < TagPoolItem::MAX_REF) {
			assert(i.ref > 0);
			++i.ref;
			return &i.item;
		}
	}

	auto *pool_item = TagPoolItem::Create(type, value);
	list.push_front(*pool_item);
	return &pool_item->item;
}

TagItem *
tag_pool_dup_item(TagItem *item) noexcept
{
	TagPoolItem *pool_item = TagItemToPoolItem(item);

	assert(pool_item->ref > 0);

	if (pool_item->ref < TagPoolItem::MAX_REF) {
		++pool_item->ref;
		return item;
	} else {
		/* the reference counter overflows above MAX_REF;
		   obtain a reference to a different TagPoolItem which
		   isn't yet "full" */
		return tag_pool_get_item(item->type, item->value);
	}
}

void
tag_pool_put_item(TagItem *item) noexcept
{
	TagPoolItem *const pool_item = TagItemToPoolItem(item);
	assert(pool_item->ref > 0);
	--pool_item->ref;

	if (pool_item->ref > 0)
		return;

	pool_item->list_hook.unlink();
	DeleteVarSize(pool_item);
}
