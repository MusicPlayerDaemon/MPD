/* the Music Player Daemon (MPD)
 * (c)2003-2004 by Warren Dukes (shank@mercury.chem.pitt.edu
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

#include <string.h>

#include <stdio.h>
#ifdef HAVE_ID3TAG
#ifdef USE_MPD_ID3TAG
#include "libid3tag/id3tag.h"
#else
#include <id3tag.h>
#endif
#endif

#define TAG_ITEM_ARTIST		0
#define TAG_ITEM_ALBUM		1
#define TAG_ITEM_TITLE		2
#define TAG_ITEM_TRACK		3
#define TAG_ITEM_NAME		4
#define TAG_ITEM_GENRE		5
#define TAG_ITEM_DATE		6

#define TAG_NUM_OF_ITEM_TYPES	7

extern char * mpdTagItemKeys[];

typedef struct _MpdTagItem {
	mpd_sint8 type;
	char * value;
} MpdTagItem;

typedef struct _MpdTag {
	int time;
	MpdTagItem * items;
	mpd_uint8 numOfItems;
} MpdTag;

#ifdef HAVE_ID3TAG
MpdTag * parseId3Tag(struct id3_tag *);
#endif

MpdTag * id3Dup(char * file);

MpdTag * newMpdTag();

void initTagConfig();

void clearItemsFromMpdTag(MpdTag * tag, int itemType);

void clearMpdTag(MpdTag * tag);

void freeMpdTag(MpdTag * tag);

void addItemToMpdTagWithLen(MpdTag * tag, int itemType, char * value, int len);

#define addItemToMpdTag(tag, itemType, value) \
		addItemToMpdTagWithLen(tag, itemType, value, strlen(value))

void printMpdTag(FILE * fp, MpdTag * tag);

MpdTag * mpdTagDup(MpdTag * tag);

int mpdTagsAreEqual(MpdTag * tag1, MpdTag * tag2);

/* *last shoudl be initialzed to -1 before calling this function */
char * getNextItemFromMpdTag(MpdTag * tag, int itemType, int * last);

#endif
