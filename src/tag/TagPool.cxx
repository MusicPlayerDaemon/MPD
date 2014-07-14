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
#include "TagPool.hxx"
#include "TagItem.hxx"
#include "util/Cast.hxx"
#include "util/VarSize.hxx"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

Mutex tag_pool_lock;

static constexpr size_t NUM_SLOTS = 4096;

struct TagPoolSlot {
	TagPoolSlot *next;
	unsigned char ref;
	TagItem item;

	TagPoolSlot(TagPoolSlot *_next, TagType type,
		    const char *value, size_t length)
		:next(_next), ref(1) {
		item.type = type;
		memcpy(item.value, value, length);
		item.value[length] = 0;
	}

	static TagPoolSlot *Create(TagPoolSlot *_next, TagType type,
				   const char *value, size_t length);
} gcc_packed;

TagPoolSlot *
TagPoolSlot::Create(TagPoolSlot *_next, TagType type,
		    const char *value, size_t length)
{
	TagPoolSlot *dummy;
	return NewVarSize<TagPoolSlot>(sizeof(dummy->item.value),
				       length + 1,
				       _next, type,
				       value, length);
}

static TagPoolSlot *slots[NUM_SLOTS];

static inline unsigned
calc_hash_n(TagType type, const char *p, size_t length)
{
	unsigned hash = 5381;

	assert(p != nullptr);

	while (length-- > 0)
		hash = (hash << 5) + hash + *p++;

	return hash ^ type;
}

static inline unsigned
calc_hash(TagType type, const char *p)
{
	unsigned hash = 5381;

	assert(p != nullptr);

	while (*p != 0)
		hash = (hash << 5) + hash + *p++;

	return hash ^ type;
}

#if defined(__clang__) || GCC_CHECK_VERSION(4,7)
	constexpr
#endif
static inline TagPoolSlot *
tag_item_to_slot(TagItem *item)
{
	return &ContainerCast(*item, &TagPoolSlot::item);
}

static inline TagPoolSlot **
tag_value_slot_p(TagType type, const char *value, size_t length)
{
	return &slots[calc_hash_n(type, value, length) % NUM_SLOTS];
}

static inline TagPoolSlot **
tag_value_slot_p(TagType type, const char *value)
{
	return &slots[calc_hash(type, value) % NUM_SLOTS];
}

TagItem *
tag_pool_get_item(TagType type, const char *value, size_t length)
{
	auto slot_p = tag_value_slot_p(type, value, length);
	for (auto slot = *slot_p; slot != nullptr; slot = slot->next) {
		if (slot->item.type == type &&
		    length == strlen(slot->item.value) &&
		    memcmp(value, slot->item.value, length) == 0 &&
		    slot->ref < 0xff) {
			assert(slot->ref > 0);
			++slot->ref;
			return &slot->item;
		}
	}

	auto slot = TagPoolSlot::Create(*slot_p, type, value, length);
	*slot_p = slot;
	return &slot->item;
}

TagItem *
tag_pool_dup_item(TagItem *item)
{
	TagPoolSlot *slot = tag_item_to_slot(item);

	assert(slot->ref > 0);

	if (slot->ref < 0xff) {
		++slot->ref;
		return item;
	} else {
		/* the reference counter overflows above 0xff;
		   duplicate the item, and start with 1 */
		size_t length = strlen(item->value);
		auto slot_p = tag_value_slot_p(item->type,
					       item->value, length);
		slot = TagPoolSlot::Create(*slot_p, item->type,
					   item->value, strlen(item->value));
		*slot_p = slot;
		return &slot->item;
	}
}

void
tag_pool_put_item(TagItem *item)
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
