/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_LOCATE_H
#define MPD_LOCATE_H

#include "gcc.h"

#include <stdint.h>
#include <stdbool.h>

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

struct song;

/* struct used for search, find, list queries */
struct locate_item {
	int8_t tag;
	/* what we are looking for */
	char *needle;
};

/**
 * An array of struct locate_item objects.
 */
struct locate_item_list {
	/** number of items */
	unsigned length;

	/** this is a variable length array */
	struct locate_item items[1];
};

int
locate_parse_type(const char *str);

/**
 * Allocates a new struct locate_item_list, and initializes all
 * members with zero bytes.
 */
struct locate_item_list *
locate_item_list_new(unsigned length);

/* return number of items or -1 on error */
gcc_nonnull(1)
struct locate_item_list *
locate_item_list_parse(char *argv[], int argc);

/**
 * Duplicate the struct locate_item_list object and convert all
 * needles with g_utf8_casefold().
 */
gcc_nonnull(1)
struct locate_item_list *
locate_item_list_casefold(const struct locate_item_list *list);

gcc_nonnull(1)
void
locate_item_list_free(struct locate_item_list *list);

gcc_nonnull(1)
void
locate_item_free(struct locate_item *item);

gcc_nonnull(1,2)
bool
locate_song_search(const struct song *song,
		   const struct locate_item_list *criteria);

gcc_nonnull(1,2)
bool
locate_song_match(const struct song *song,
		   const struct locate_item_list *criteria);

#endif
