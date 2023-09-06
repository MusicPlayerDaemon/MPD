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

struct TagPoolSlot {
	IntrusiveListHook<IntrusiveHookMode::NORMAL> list_hook;
	uint8_t ref = 1;
	TagItem item;

	static constexpr unsigned MAX_REF = std::numeric_limits<decltype(ref)>::max();

	TagPoolSlot(TagType type,
		    std::string_view value) noexcept {
		item.type = type;
		*std::copy(value.begin(), value.end(), item.value) = 0;
	}

	static TagPoolSlot *Create(TagType type,
				   std::string_view value) noexcept;
};

TagPoolSlot *
TagPoolSlot::Create(TagType type,
		    std::string_view value) noexcept
{
	TagPoolSlot *dummy;
	return NewVarSize<TagPoolSlot>(sizeof(dummy->item.value),
				       value.size() + 1,
				       type,
				       value);
}

static std::array<IntrusiveList<TagPoolSlot,
				IntrusiveListMemberHookTraits<&TagPoolSlot::list_hook>>,
		  16127> slots;

static inline unsigned
calc_hash(TagType type, std::string_view p) noexcept
{
	unsigned hash = 5381;

	for (auto ch : p)
		hash = (hash << 5) + hash + ch;

	return hash ^ type;
}

static constexpr TagPoolSlot *
tag_item_to_slot(TagItem *item) noexcept
{
	return &ContainerCast(*item, &TagPoolSlot::item);
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

	for (auto &slot : list) {
		if (slot.item.type == type &&
		    value == slot.item.value &&
		    slot.ref < TagPoolSlot::MAX_REF) {
			assert(slot.ref > 0);
			++slot.ref;
			return &slot.item;
		}
	}

	auto slot = TagPoolSlot::Create(type, value);
	list.push_front(*slot);
	return &slot->item;
}

TagItem *
tag_pool_dup_item(TagItem *item) noexcept
{
	TagPoolSlot *slot = tag_item_to_slot(item);

	assert(slot->ref > 0);

	if (slot->ref < TagPoolSlot::MAX_REF) {
		++slot->ref;
		return item;
	} else {
		/* the reference counter overflows above MAX_REF;
		   obtain a reference to a different TagPoolSlot which
		   isn't yet "full" */
		return tag_pool_get_item(item->type, item->value);
	}
}

void
tag_pool_put_item(TagItem *item) noexcept
{
	TagPoolSlot *const slot = tag_item_to_slot(item);
	assert(slot->ref > 0);
	--slot->ref;

	if (slot->ref > 0)
		return;

	slot->list_hook.unlink();
	DeleteVarSize(slot);
}
