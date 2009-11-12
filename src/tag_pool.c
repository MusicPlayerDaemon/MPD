/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "tag_pool.h"

#include <assert.h>

GMutex *tag_pool_lock = NULL;

#define NUM_SLOTS 4096

struct slot {
	struct slot *next;
	unsigned char ref;
	struct tag_item item;
} mpd_packed;

static struct slot *slots[NUM_SLOTS];

static inline unsigned
calc_hash_n(enum tag_type type, const char *p, size_t length)
{
	unsigned hash = 5381;

	assert(p != NULL);

	while (length-- > 0)
		hash = (hash << 5) + hash + *p++;

	return hash ^ type;
}

static inline unsigned
calc_hash(enum tag_type type, const char *p)
{
	unsigned hash = 5381;

	assert(p != NULL);

	while (*p != 0)
		hash = (hash << 5) + hash + *p++;

	return hash ^ type;
}

static inline struct slot *
tag_item_to_slot(struct tag_item *item)
{
	return (struct slot*)(((char*)item) - offsetof(struct slot, item));
}

static struct slot *slot_alloc(struct slot *next,
			       enum tag_type type,
			       const char *value, int length)
{
	struct slot *slot;

	slot = g_malloc(sizeof(*slot) - sizeof(slot->item.value) + length + 1);
	slot->next = next;
	slot->ref = 1;
	slot->item.type = type;
	memcpy(slot->item.value, value, length);
	slot->item.value[length] = 0;
	return slot;
}

void tag_pool_init(void)
{
	g_assert(tag_pool_lock == NULL);
	tag_pool_lock = g_mutex_new();
}

void tag_pool_deinit(void)
{
	g_assert(tag_pool_lock != NULL);
	g_mutex_free(tag_pool_lock);
	tag_pool_lock = NULL;
}

struct tag_item *
tag_pool_get_item(enum tag_type type, const char *value, size_t length)
{
	struct slot **slot_p, *slot;

	slot_p = &slots[calc_hash_n(type, value, length) % NUM_SLOTS];
	for (slot = *slot_p; slot != NULL; slot = slot->next) {
		if (slot->item.type == type &&
		    length == strlen(slot->item.value) &&
		    memcmp(value, slot->item.value, length) == 0 &&
		    slot->ref < 0xff) {
			assert(slot->ref > 0);
			++slot->ref;
			return &slot->item;
		}
	}

	slot = slot_alloc(*slot_p, type, value, length);
	*slot_p = slot;
	return &slot->item;
}

struct tag_item *tag_pool_dup_item(struct tag_item *item)
{
	struct slot *slot = tag_item_to_slot(item);

	assert(slot->ref > 0);

	if (slot->ref < 0xff) {
		++slot->ref;
		return item;
	} else {
		/* the reference counter overflows above 0xff;
		   duplicate the item, and start with 1 */
		size_t length = strlen(item->value);
		struct slot **slot_p =
			&slots[calc_hash_n(item->type, item->value,
					   length) % NUM_SLOTS];
		slot = slot_alloc(*slot_p, item->type,
				  item->value, strlen(item->value));
		*slot_p = slot;
		return &slot->item;
	}
}

void tag_pool_put_item(struct tag_item *item)
{
	struct slot **slot_p, *slot;

	slot = tag_item_to_slot(item);
	assert(slot->ref > 0);
	--slot->ref;

	if (slot->ref > 0)
		return;

	for (slot_p = &slots[calc_hash(item->type, item->value) % NUM_SLOTS];
	     *slot_p != slot;
	     slot_p = &(*slot_p)->next) {
		assert(*slot_p != NULL);
	}

	*slot_p = slot->next;
	g_free(slot);
}
