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

#include "../config.h"

#include "mpd_types.h"

#ifdef HAVE_ID3TAG
#include <id3tag.h>
#endif

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

typedef struct _MpdTagItem {
	enum tag_type type;
	char *value;
} MpdTagItem;

typedef struct _MpdTag {
	int time;
	MpdTagItem *items;
	mpd_uint8 numOfItems;
} MpdTag;

#ifdef HAVE_ID3TAG
MpdTag *parseId3Tag(struct id3_tag *);
#endif

MpdTag *apeDup(char *file);

MpdTag *id3Dup(char *file);

MpdTag *newMpdTag(void);

void initTagConfig(void);

void clearItemsFromMpdTag(MpdTag * tag, enum tag_type itemType);

void freeMpdTag(MpdTag * tag);

void addItemToMpdTagWithLen(MpdTag * tag, enum tag_type itemType,
			    char *value, int len);

#define addItemToMpdTag(tag, itemType, value) \
		addItemToMpdTagWithLen(tag, itemType, value, strlen(value))

void printTagTypes(int fd);

void printMpdTag(int fd, MpdTag * tag);

MpdTag *mpdTagDup(MpdTag * tag);

int mpdTagsAreEqual(MpdTag * tag1, MpdTag * tag2);

#endif
