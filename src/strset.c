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
#include "strset.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define NUM_SLOTS 16384

struct strset_slot {
	struct strset_slot *next;
	const char *value;
};

struct strset {
	unsigned size;

	struct strset_slot *current_slot;
	unsigned next_slot;

	struct strset_slot slots[NUM_SLOTS];
};

static unsigned calc_hash(const char *p) {
	unsigned hash = 5381;

	assert(p != NULL);

	while (*p != 0)
		hash = (hash << 5) + hash + *p++;

	return hash;
}

G_GNUC_MALLOC struct strset *strset_new(void)
{
	struct strset *set = g_new0(struct strset, 1);
	return set;
}

void strset_free(struct strset *set)
{
	unsigned i;

	for (i = 0; i < NUM_SLOTS; ++i) {
		struct strset_slot *slot = set->slots[i].next, *next;

		while (slot != NULL) {
			next = slot->next;
			g_free(slot);
			slot = next;
		}
	}

	g_free(set);
}

void strset_add(struct strset *set, const char *value)
{
	struct strset_slot *base_slot
		= &set->slots[calc_hash(value) % NUM_SLOTS];
	struct strset_slot *slot = base_slot;

	if (base_slot->value == NULL) {
		/* empty slot - put into base_slot */
		assert(base_slot->next == NULL);

		base_slot->value = value;
		++set->size;
		return;
	}

	for (slot = base_slot; slot != NULL; slot = slot->next)
		if (strcmp(slot->value, value) == 0)
			/* found it - do nothing */
			return;

	/* insert it into the slot chain */
	slot = g_new(struct strset_slot, 1);
	slot->next = base_slot->next;
	slot->value = value;
	base_slot->next = slot;
	++set->size;
}

int strset_get(const struct strset *set, const char *value)
{
	const struct strset_slot *slot = &set->slots[calc_hash(value)];

	if (slot->value == NULL)
		return 0;

	for (slot = slot->next; slot != NULL; slot = slot->next)
		if (strcmp(slot->value, value) == 0)
			/* found it - do nothing */
			return 1;

	return 0;
}

unsigned strset_size(const struct strset *set)
{
	return set->size;
}

void strset_rewind(struct strset *set)
{
	set->current_slot = NULL;
	set->next_slot = 0;
}

const char *strset_next(struct strset *set)
{
	if (set->current_slot != NULL && set->current_slot->next != NULL) {
		set->current_slot = set->current_slot->next;
		return set->current_slot->value;
	}

	while (set->next_slot < NUM_SLOTS &&
	       set->slots[set->next_slot].value == NULL)
		++set->next_slot;

	if (set->next_slot >= NUM_SLOTS)
		return NULL;

	set->current_slot = &set->slots[set->next_slot++];
	return set->current_slot->value;
}

