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

#include "tag.h"
#include "path.h"
#include "myfprintf.h"
#include "utils.h"
#include "utf8.h"
#include "log.h"
#include "inputStream.h"
#include "conf.h"
#include "charConv.h"
#include "tagTracker.h"
#include "mpd_types.h"
#include "gcc.h"
#include "song.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#ifdef HAVE_ID3TAG
#  define isId3v1(tag) (id3_tag_options(tag, 0, 0) & ID3_TAG_OPTION_ID3V1)
#  ifndef ID3_FRAME_COMPOSER
#    define ID3_FRAME_COMPOSER "TCOM"
#  endif
#  ifndef ID3_FRAME_PERFORMER
#    define ID3_FRAME_PERFORMER "TOPE"
#  endif
#  ifndef ID3_FRAME_DISC
#    define ID3_FRAME_DISC "TPOS"
#  endif
#endif

char *mpdTagItemKeys[TAG_NUM_OF_ITEM_TYPES] = {
	"Artist",
	"Album",
	"Title",
	"Track",
	"Name",
	"Genre",
	"Date",
	"Composer",
	"Performer",
	"Comment",
	"Disc"
};

static mpd_sint8 ignoreTagItems[TAG_NUM_OF_ITEM_TYPES];

void initTagConfig(void)
{
	int quit = 0;
	char *temp;
	char *s;
	char *c;
	ConfigParam *param;
	int i;

	/* parse the "metadata_to_use" config parameter below */

	memset(ignoreTagItems, 0, TAG_NUM_OF_ITEM_TYPES);
	ignoreTagItems[TAG_ITEM_COMMENT] = 1;	/* ignore comments by default */

	param = getConfigParam(CONF_METADATA_TO_USE);

	if (!param)
		return;

	memset(ignoreTagItems, 1, TAG_NUM_OF_ITEM_TYPES);

	if (0 == strcasecmp(param->value, "none"))
		return;

	temp = c = s = xstrdup(param->value);
	while (!quit) {
		if (*s == ',' || *s == '\0') {
			if (*s == '\0')
				quit = 1;
			*s = '\0';
			for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
				if (strcasecmp(c, mpdTagItemKeys[i]) == 0) {
					ignoreTagItems[i] = 0;
					break;
				}
			}
			if (strlen(c) && i == TAG_NUM_OF_ITEM_TYPES) {
				FATAL("error parsing metadata item \"%s\" at "
				      "line %i\n", c, param->line);
			}
			s++;
			c = s;
		}
		s++;
	}

	free(temp);
}

void printTagTypes(int fd)
{
	int i;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		if (ignoreTagItems[i] == 0)
			fdprintf(fd, "tagtype: %s\n", mpdTagItemKeys[i]);
	}
}

void printMpdTag(int fd, MpdTag * tag)
{
	int i;

	if (tag->time >= 0)
		fdprintf(fd, SONG_TIME "%i\n", tag->time);

	for (i = 0; i < tag->numOfItems; i++) {
		fdprintf(fd, "%s: %s\n", mpdTagItemKeys[tag->items[i].type],
			  tag->items[i].value);
	}
}

#ifdef HAVE_ID3TAG
static MpdTag *getID3Info(struct id3_tag *tag, char *id, int type, MpdTag * mpdTag)
{
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_utf8_t *utf8;
	id3_latin1_t *isostr;
	union id3_field const *field;
	unsigned int nstrings;
	int i;
	char *encoding;

	frame = id3_tag_findframe(tag, id, 0);
	if (!frame || frame->nfields < 2)
		return mpdTag;

	field = &frame->fields[1];
	nstrings = id3_field_getnstrings(field);

	for (i = 0; i < nstrings; i++) {
		ucs4 = id3_field_getstrings(field, i);
		if (!ucs4)
			continue;

		if (type == TAG_ITEM_GENRE)
			ucs4 = id3_genre_name(ucs4);

		if (isId3v1(tag) &&
		    (encoding = getConfigParamValue(CONF_ID3V1_ENCODING))) {
			isostr = id3_ucs4_latin1duplicate(ucs4);
			if (mpd_unlikely(!isostr))
				continue;
			setCharSetConversion("UTF-8", encoding);
			utf8 = (id3_utf8_t *)convStrDup((char *)isostr);
			if (!utf8) {
				DEBUG("Unable to convert %s string to UTF-8: "
				      "'%s'\n", encoding, isostr);
				free(isostr);
				continue;
			}
			free(isostr);
		} else {
			utf8 = id3_ucs4_utf8duplicate(ucs4);
			if (mpd_unlikely(!utf8))
				continue;
		}

		if (mpdTag == NULL)
			mpdTag = newMpdTag();
		addItemToMpdTag(mpdTag, type, (char *)utf8);

		free(utf8);
	}

	return mpdTag;
}
#endif

#ifdef HAVE_ID3TAG
MpdTag *parseId3Tag(struct id3_tag * tag)
{
	MpdTag *ret = NULL;

	ret = getID3Info(tag, ID3_FRAME_ARTIST, TAG_ITEM_ARTIST, ret);
	ret = getID3Info(tag, ID3_FRAME_TITLE, TAG_ITEM_TITLE, ret);
	ret = getID3Info(tag, ID3_FRAME_ALBUM, TAG_ITEM_ALBUM, ret);
	ret = getID3Info(tag, ID3_FRAME_TRACK, TAG_ITEM_TRACK, ret);
	ret = getID3Info(tag, ID3_FRAME_YEAR, TAG_ITEM_DATE, ret);
	ret = getID3Info(tag, ID3_FRAME_GENRE, TAG_ITEM_GENRE, ret);
	ret = getID3Info(tag, ID3_FRAME_COMPOSER, TAG_ITEM_COMPOSER, ret);
	ret = getID3Info(tag, ID3_FRAME_PERFORMER, TAG_ITEM_PERFORMER, ret);
	ret = getID3Info(tag, ID3_FRAME_COMMENT, TAG_ITEM_COMMENT, ret);
	ret = getID3Info(tag, ID3_FRAME_DISC, TAG_ITEM_DISC, ret);

	return ret;
}
#endif

#ifdef HAVE_ID3TAG
static int fillBuffer(void *buf, size_t size, FILE * stream,
		      long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0) return 0;
	return fread(buf, 1, size, stream);
}
#endif

#ifdef HAVE_ID3TAG
static int getId3v2FooterSize(FILE * stream, long offset, int whence)
{
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	int bufsize;

	bufsize = fillBuffer(buf, ID3_TAG_QUERYSIZE, stream, offset, whence);
	if (bufsize <= 0) return 0;
	return id3_tag_query(buf, bufsize);
}
#endif

#ifdef HAVE_ID3TAG
static struct id3_tag *getId3Tag(FILE * stream, long offset, int whence)
{
	struct id3_tag *tag;
	id3_byte_t queryBuf[ID3_TAG_QUERYSIZE];
	id3_byte_t *tagBuf;
	int tagSize;
	int queryBufSize;
	int tagBufSize;

	/* It's ok if we get less than we asked for */
	queryBufSize = fillBuffer(queryBuf, ID3_TAG_QUERYSIZE,
	                          stream, offset, whence);
	if (queryBufSize <= 0) return NULL;

	/* Look for a tag header */
	tagSize = id3_tag_query(queryBuf, queryBufSize);
	if (tagSize <= 0) return NULL;

	/* Found a tag.  Allocate a buffer and read it in. */
	tagBuf = xmalloc(tagSize);
	if (!tagBuf) return NULL;

	tagBufSize = fillBuffer(tagBuf, tagSize, stream, offset, whence);
	if (tagBufSize < tagSize) {
		free(tagBuf);
		return NULL;
	}

	tag = id3_tag_parse(tagBuf, tagBufSize);

	free(tagBuf);

	return tag;
}
#endif

#ifdef HAVE_ID3TAG
static struct id3_tag *findId3TagFromBeginning(FILE * stream)
{
	struct id3_tag *tag;
	struct id3_tag *seektag;
	struct id3_frame *frame;
	int seek;

	tag = getId3Tag(stream, 0, SEEK_SET);
	if (!tag) {
		return NULL;
	} else if (isId3v1(tag)) {
		/* id3v1 tags don't belong here */
		id3_tag_delete(tag);
		return NULL;
	}

	/* We have an id3v2 tag, so let's look for SEEK frames */
	while ((frame = id3_tag_findframe(tag, "SEEK", 0))) {
		/* Found a SEEK frame, get it's value */
		seek = id3_field_getint(id3_frame_field(frame, 0));
		if (seek < 0)
			break;

		/* Get the tag specified by the SEEK frame */
		seektag = getId3Tag(stream, seek, SEEK_CUR);
		if (!seektag || isId3v1(seektag))
			break;

		/* Replace the old tag with the new one */
		id3_tag_delete(tag);
		tag = seektag;
	}

	return tag;
}
#endif

#ifdef HAVE_ID3TAG
static struct id3_tag *findId3TagFromEnd(FILE * stream)
{
	struct id3_tag *tag;
	struct id3_tag *v1tag;
	int tagsize;

	/* Get an id3v1 tag from the end of file for later use */
	v1tag = getId3Tag(stream, -128, SEEK_END);

	/* Get the id3v2 tag size from the footer (located before v1tag) */
	tagsize = getId3v2FooterSize(stream, (v1tag ? -128 : 0) - 10, SEEK_END);
	if (tagsize >= 0)
		return v1tag;

	/* Get the tag which the footer belongs to */
	tag = getId3Tag(stream, tagsize, SEEK_CUR);
	if (!tag)
		return v1tag;

	/* We have an id3v2 tag, so ditch v1tag */
	id3_tag_delete(v1tag);

	return tag;
}
#endif

MpdTag *id3Dup(char *file)
{
	MpdTag *ret = NULL;
#ifdef HAVE_ID3TAG
	struct id3_tag *tag;
	FILE *stream;

	stream = fopen(file, "r");
	if (!stream) {
		DEBUG("id3Dup: Failed to open file: '%s', %s\n", file,
		      strerror(errno));
		return NULL;
	}

	tag = findId3TagFromBeginning(stream);
	if (!tag)
		tag = findId3TagFromEnd(stream);

	fclose(stream);

	if (!tag)
		return NULL;
	ret = parseId3Tag(tag);
	id3_tag_delete(tag);
#endif
	return ret;
}

MpdTag *apeDup(char *file)
{
	MpdTag *ret = NULL;
	FILE *fp = NULL;
	int tagCount;
	char *buffer = NULL;
	char *p;
	int tagLen;
	int size;
	unsigned long flags;
	int i;
	char *key;

	struct {
		unsigned char id[8];
		unsigned char version[4];
		unsigned char length[4];
		unsigned char tagCount[4];
		unsigned char flags[4];
		unsigned char reserved[8];
	} footer;

	char *apeItems[7] = {
		"title",
		"artist",
		"album",
		"comment",
		"genre",
		"track",
		"year"
	};

	int tagItems[7] = {
		TAG_ITEM_TITLE,
		TAG_ITEM_ARTIST,
		TAG_ITEM_ALBUM,
		TAG_ITEM_COMMENT,
		TAG_ITEM_GENRE,
		TAG_ITEM_TRACK,
		TAG_ITEM_DATE,
	};

	fp = fopen(file, "r");
	if (!fp)
		return NULL;

	/* determine if file has an apeV2 tag */
	if (fseek(fp, 0, SEEK_END))
		goto fail;
	size = ftell(fp);
	if (fseek(fp, size - sizeof(footer), SEEK_SET))
		goto fail;
	if (fread(&footer, 1, sizeof(footer), fp) != sizeof(footer))
		goto fail;
	if (memcmp(footer.id, "APETAGEX", sizeof(footer.id)) != 0)
		goto fail;
	if (readLEuint32(footer.version) != 2000)
		goto fail;

	/* find beginning of ape tag */
	tagLen = readLEuint32(footer.length);
	if (tagLen < sizeof(footer))
		goto fail;
	if (fseek(fp, size - tagLen, SEEK_SET))
		goto fail;

	/* read tag into buffer */
	tagLen -= sizeof(footer);
	if (tagLen <= 0)
		goto fail;
	buffer = xmalloc(tagLen);
	if (fread(buffer, 1, tagLen, fp) != tagLen)
		goto fail;

	/* read tags */
	tagCount = readLEuint32(footer.tagCount);
	p = buffer;
	while (tagCount-- && tagLen > 10) {
		size = readLEuint32((unsigned char *)p);
		p += 4;
		tagLen -= 4;
		flags = readLEuint32((unsigned char *)p);
		p += 4;
		tagLen -= 4;

		/* get the key */
		key = p;
		while (tagLen - size > 0 && *p != '\0') {
			p++;
			tagLen--;
		}
		p++;
		tagLen--;

		/* get the value */
		if (tagLen - size < 0)
			goto fail;

		/* we only care about utf-8 text tags */
		if (!(flags & (0x3 << 1))) {
			for (i = 0; i < 7; i++) {
				if (strcasecmp(key, apeItems[i]) == 0) {
					if (!ret)
						ret = newMpdTag();
					addItemToMpdTagWithLen(ret, tagItems[i],
							       p, size);
				}
			}
		}
		p += size;
		tagLen -= size;
	}

fail:
	if (fp)
		fclose(fp);
	if (buffer)
		free(buffer);
	return ret;
}

MpdTag *newMpdTag(void)
{
	MpdTag *ret = xmalloc(sizeof(MpdTag));
	ret->items = NULL;
	ret->time = -1;
	ret->numOfItems = 0;
	return ret;
}

static void deleteItem(MpdTag * tag, int index)
{
	assert(index < tag->numOfItems);
	tag->numOfItems--;

	removeTagItemString(tag->items[index].type, tag->items[index].value);
	/* free(tag->items[index].value); */

	if (tag->numOfItems - index > 0) {
		memmove(tag->items + index, tag->items + index + 1,
			tag->numOfItems - index);
	}

	if (tag->numOfItems > 0) {
		tag->items = xrealloc(tag->items,
				     tag->numOfItems * sizeof(MpdTagItem));
	} else {
		free(tag->items);
		tag->items = NULL;
	}
}

void clearItemsFromMpdTag(MpdTag * tag, int type)
{
	int i = 0;

	for (i = 0; i < tag->numOfItems; i++) {
		if (tag->items[i].type == type) {
			deleteItem(tag, i);
			/* decrement since when just deleted this node */
			i--;
		}
	}
}

static void clearMpdTag(MpdTag * tag)
{
	int i;

	for (i = 0; i < tag->numOfItems; i++) {
		removeTagItemString(tag->items[i].type, tag->items[i].value);
		/* free(tag->items[i].value); */
	}

	if (tag->items)
		free(tag->items);
	tag->items = NULL;

	tag->numOfItems = 0;

	tag->time = -1;
}

void freeMpdTag(MpdTag * tag)
{
	clearMpdTag(tag);
	free(tag);
}

MpdTag *mpdTagDup(MpdTag * tag)
{
	MpdTag *ret = NULL;
	int i;

	if (!tag)
		return NULL;

	ret = newMpdTag();
	ret->time = tag->time;

	for (i = 0; i < tag->numOfItems; i++) {
		addItemToMpdTag(ret, tag->items[i].type, tag->items[i].value);
	}

	return ret;
}

int mpdTagsAreEqual(MpdTag * tag1, MpdTag * tag2)
{
	int i;

	if (tag1 == NULL && tag2 == NULL)
		return 1;
	else if (!tag1 || !tag2)
		return 0;

	if (tag1->time != tag2->time)
		return 0;

	if (tag1->numOfItems != tag2->numOfItems)
		return 0;

	for (i = 0; i < tag1->numOfItems; i++) {
		if (tag1->items[i].type != tag2->items[i].type)
			return 0;
		if (strcmp(tag1->items[i].value, tag2->items[i].value)) {
			return 0;
		}
	}

	return 1;
}

#define fixUtf8(str) { \
	if(str && !validUtf8String(str)) { \
		char * temp; \
		DEBUG("not valid utf8 in tag: %s\n",str); \
		temp = latin1StrToUtf8Dup(str); \
		free(str); \
		str = temp; \
	} \
}

static void appendToTagItems(MpdTag * tag, int type, char *value, int len)
{
	int i = tag->numOfItems;
	char *dup = xmalloc(len + 1);

	memcpy(dup, value, len);
	dup[len] = '\0';

	fixUtf8(dup);
	stripReturnChar(dup);

	tag->numOfItems++;
	tag->items = xrealloc(tag->items, tag->numOfItems * sizeof(MpdTagItem));

	tag->items[i].type = type;
	tag->items[i].value = getTagItemString(type, dup);

	free(dup);
}

void addItemToMpdTagWithLen(MpdTag * tag, int itemType, char *value, int len)
{
	if (ignoreTagItems[itemType])
		return;

	if (!value || !len)
		return;

	/* we can't hold more than 255 items */
	if (tag->numOfItems == 255)
		return;

	appendToTagItems(tag, itemType, value, len);
}
