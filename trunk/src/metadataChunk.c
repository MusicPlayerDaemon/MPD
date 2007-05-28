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

#include "metadataChunk.h"
#include "gcc.h"

#include <string.h>

static void initMetadataChunk(MetadataChunk * chunk)
{
	chunk->name = -1;
	chunk->artist = -1;
	chunk->album = -1;
	chunk->title = -1;
}

#define dupElementToTag(item, element) { \
	if(element >= 0 && element < METADATA_BUFFER_LENGTH) { \
		addItemToMpdTag(ret, item, chunk->buffer+element); \
	} \
}

MpdTag *metadataChunkToMpdTagDup(MetadataChunk * chunk)
{
	MpdTag *ret = newMpdTag();

	chunk->buffer[METADATA_BUFFER_LENGTH - 1] = '\0';

	dupElementToTag(TAG_ITEM_NAME, chunk->name);
	dupElementToTag(TAG_ITEM_TITLE, chunk->title);
	dupElementToTag(TAG_ITEM_ARTIST, chunk->artist);
	dupElementToTag(TAG_ITEM_ALBUM, chunk->album);

	return ret;
}

#define copyStringToChunk(string, element) { \
	if(element < 0 && string && (slen = strlen(string)) && \
                        pos < METADATA_BUFFER_LENGTH-1) \
        { \
		size_t len = slen; \
		size_t max = METADATA_BUFFER_LENGTH - 1 - pos; \
		if (mpd_unlikely(len > max)) \
			len = max; \
		memcpy(chunk->buffer+pos, string, len); \
		*(chunk->buffer+pos+len) = '\0'; \
		element = pos; \
		pos += slen+1; \
	} \
}

void copyMpdTagToMetadataChunk(MpdTag * tag, MetadataChunk * chunk)
{
	int pos = 0;
	int slen;
	int i;

	initMetadataChunk(chunk);

	if (!tag)
		return;

	for (i = 0; i < tag->numOfItems; i++) {
		switch (tag->items[i].type) {
		case TAG_ITEM_NAME:
			copyStringToChunk(tag->items[i].value, chunk->name);
			break;
		case TAG_ITEM_TITLE:
			copyStringToChunk(tag->items[i].value, chunk->title);
			break;
		case TAG_ITEM_ARTIST:
			copyStringToChunk(tag->items[i].value, chunk->artist);
			break;
		case TAG_ITEM_ALBUM:
			copyStringToChunk(tag->items[i].value, chunk->album);
			break;
		}
	}
}
