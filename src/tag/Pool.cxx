/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Pool.hxx"
#include "Item.hxx"
#include "util/Cast.hxx"
#include "util/VarSize.hxx"
#include "util/StringView.hxx"

#include <cassert>
#include <cstdint>
#include <limits>

#include <string.h>
#include <stdlib.h>

Mutex tag_pool_lock;

static constexpr size_t NUM_SLOTS = 16127;

struct TagPoolSlot {
	TagPoolSlot *next;
	uint8_t ref = 1;
	TagItem item;

	static constexpr unsigned MAX_REF = std::numeric_limits<decltype(ref)>::max();

	TagPoolSlot(TagPoolSlot *_next, TagType type,
		    StringView value) noexcept
		:next(_next) {
		item.type = type;
		memcpy(item.value, value.data, value.size);
		item.value[value.size] = 0;
	}

	static TagPoolSlot *Create(TagPoolSlot *_next, TagType type,
				   StringView value) noexcept;
};

TagPoolSlot *
TagPoolSlot::Create(TagPoolSlot *_next, TagType type,
		    StringView value) noexcept
{
	TagPoolSlot *dummy;
	return NewVarSize<TagPoolSlot>(sizeof(dummy->item.value),
				       value.size + 1,
				       _next, type,
				       value);
}

static TagPoolSlot *slots[NUM_SLOTS];

static inline unsigned
calc_hash(TagType type, StringView p) noexcept
{
	unsigned hash = 5381;

	for (auto ch : p)
		hash = (hash << 5) + hash + ch;

	return hash ^ type;
}

static inline unsigned
calc_hash(TagType type, const char *p) noexcept
{
	unsigned hash = 5381;

	assert(p != nullptr);

	while (*p != 0)
		hash = (hash << 5) + hash + *p++;

	return hash ^ type;
}

static constexpr TagPoolSlot *
tag_item_to_slot(TagItem *item) noexcept
{
	return &ContainerCast(*item, &TagPoolSlot::item);
}

static inline TagPoolSlot **
tag_value_slot_p(TagType type, StringView value) noexcept
{
	return &slots[calc_hash(type, value) % NUM_SLOTS];
}

static inline TagPoolSlot **
tag_value_slot_p(TagType type, const char *value) noexcept
{
	return &slots[calc_hash(type, value) % NUM_SLOTS];
}

TagItem *
tag_pool_get_item(TagType type, StringView value) noexcept
{
	auto slot_p = tag_value_slot_p(type, value);
	for (auto slot = *slot_p; slot != nullptr; slot = slot->next) {
		if (slot->item.type == type &&
		    value.Equals(slot->item.value) &&
		    slot->ref < TagPoolSlot::MAX_REF) {
			assert(slot->ref > 0);
			++slot->ref;
			return &slot->item;
		}
	}

	auto slot = TagPoolSlot::Create(*slot_p, type, value);
	*slot_p = slot;
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
	TagPoolSlot **slot_p, *slot;

	slot = tag_item_to_slot(item);
	assert(slot->ref > 0);
	--slot->ref;

	if (slot->ref > 0)
		return;

	for (slot_p = tag_value_slot_p(item->type, item->value);
	     *slot_p != slot;
	     slot_p = &(*slot_p)->next) {
		assert(*slot_p != nullptr);
	}

	*slot_p = slot->next;
	DeleteVarSize(slot);
}
