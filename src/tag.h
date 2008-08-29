/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TAG_H
#define TAG_H

#include "mpd_types.h"
#include "os_compat.h"
#include "gcc.h"

enum tag_type {
	TAG_ITEM_ARTIST,
	TAG_ITEM_ALBUM,
	TAG_ITEM_TITLE,
	TAG_ITEM_TRACK,
	TAG_ITEM_NAME,
	TAG_ITEM_GENRE,
	TAG_ITEM_DATE,
	TAG_ITEM_COMPOSER,
	TAG_ITEM_PERFORMER,
	TAG_ITEM_COMMENT,
	TAG_ITEM_DISC,
	TAG_NUM_OF_ITEM_TYPES
};

extern const char *mpdTagItemKeys[];

struct tag_item {
	enum tag_type type;
	char value[1];
} mpd_packed;

struct tag {
	int time;
	struct tag_item **items;
	mpd_uint8 numOfItems;
};

struct tag *tag_ape_load(char *file);

struct tag *tag_new(void);

void tag_lib_init(void);

void tag_clear_items_by_type(struct tag *tag, enum tag_type itemType);

void tag_free(struct tag *tag);

void tag_add_item_n(struct tag *tag, enum tag_type itemType,
			    const char *value, size_t len);

static inline void tag_add_item(struct tag *tag, enum tag_type itemType,
				const char *value)
{
	tag_add_item_n(tag, itemType, value, strlen(value));
}

void tag_print_types(int fd);

void tag_print(int fd, struct tag *tag);

struct tag *tag_dup(struct tag *tag);

int tag_equal(struct tag *tag1, struct tag *tag2);

#endif
