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

#include "tag.h"
#include "path.h"
#include "myfprintf.h"
#include "utils.h"
#include "utf8.h"
#include "log.h"
#include "inputStream.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#ifdef HAVE_OGG
#include <vorbis/vorbisfile.h>
#endif
#ifdef HAVE_FLAC
#include <FLAC/file_decoder.h>
#include <FLAC/metadata.h>
#endif

void printMpdTag(FILE * fp, MpdTag * tag) {
	if(tag->artist) myfprintf(fp,"Artist: %s\n",tag->artist);
	if(tag->album) myfprintf(fp,"Album: %s\n",tag->album);
	if(tag->track) myfprintf(fp,"Track: %s\n",tag->track);
	if(tag->title) myfprintf(fp,"Title: %s\n",tag->title);
	if(tag->name) myfprintf(fp,"Name: %s\n",tag->name);
	if(tag->time>=0) myfprintf(fp,"Time: %i\n",tag->time);
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

void validateUtf8Tag(MpdTag * tag) {
	fixUtf8(tag->artist);
	stripReturnChar(tag->artist);
	fixUtf8(tag->album);
	stripReturnChar(tag->album);
	fixUtf8(tag->track);
	stripReturnChar(tag->track);
	fixUtf8(tag->title);
	stripReturnChar(tag->title);
	fixUtf8(tag->name);
	stripReturnChar(tag->name);
}

#ifdef HAVE_ID3TAG
char * getID3Info(struct id3_tag * tag, char * id) {
	struct id3_frame const * frame;
	id3_ucs4_t const * ucs4;
	id3_utf8_t * utf8;
	union id3_field const * field;
	unsigned int nstrings;

	frame = id3_tag_findframe(tag, id, 0);
	if(!frame) return NULL;

	field = &frame->fields[1];
	nstrings = id3_field_getnstrings(field);
	if(nstrings<1) return NULL;

	ucs4 = id3_field_getstrings(field,0);
	assert(ucs4);

	utf8 = id3_ucs4_utf8duplicate(ucs4);
	if(!utf8) return NULL;

	return utf8;
}
#endif

#ifdef HAVE_ID3TAG
MpdTag * parseId3Tag(struct id3_tag * tag) {
	MpdTag * ret = NULL;
	char * str;

	str = getID3Info(tag,ID3_FRAME_ARTIST);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->artist = str;
	}

	str = getID3Info(tag,ID3_FRAME_TITLE);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->title = str;
	}

	str = getID3Info(tag,ID3_FRAME_ALBUM);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->album = str;
	}

	str = getID3Info(tag,ID3_FRAME_TRACK);
	if(str) {
		if(!ret) ret = newMpdTag();
		ret->track = str;
	}

	return ret;
}
#endif

MpdTag * id3Dup(char * file) {
	MpdTag * ret = NULL;
#ifdef HAVE_ID3TAG
	struct id3_file * id3_file;
	struct id3_tag * tag;

	id3_file = id3_file_open(file, ID3_FILE_MODE_READONLY);
			
	if(!id3_file) {
		return NULL;
	}

	tag = id3_file_tag(id3_file);
	if(!tag) {
		id3_file_close(id3_file);
		return NULL;
	}

	ret = parseId3Tag(tag);

	id3_file_close(id3_file);

#endif
	return ret;	
}

MpdTag * newMpdTag() {
	MpdTag * ret = malloc(sizeof(MpdTag));
	ret->album = NULL;
	ret->artist = NULL;
	ret->title = NULL;
	ret->track = NULL;
	ret->name = NULL;
	ret->time = -1;
	return ret;
}

void clearMpdTag(MpdTag * tag) {
	if(tag->artist) free(tag->artist);
	if(tag->album) free(tag->album);
	if(tag->title) free(tag->title);
	if(tag->name) free(tag->name);
	if(tag->track) free(tag->track);
}

void freeMpdTag(MpdTag * tag) {
        clearMpdTag(tag);
	free(tag);
}

MpdTag * mpdTagDup(MpdTag * tag) {
	MpdTag * ret = NULL;

	if(tag) {
		ret = newMpdTag();
		if(tag->artist) ret->artist = strdup(tag->artist);
		if(tag->album) ret->album = strdup(tag->album);
		if(tag->title) ret->title = strdup(tag->title);
		if(tag->track) ret->track = strdup(tag->track);
		if(tag->name) ret->name = strdup(tag->name);
		ret->time = tag->time;
	}

	return ret;
}

int mpdTagStringsAreEqual(char * s1, char * s2) {
        if(s1 && s2) {
                if(strcmp(s1, s2)) return 0;
        }
        else if(s1 || s2) return 0;

        return 1;
}

int mpdTagsAreEqual(MpdTag * tag1, MpdTag * tag2) {
        if(tag1 == NULL && tag2 == NULL) return 1;
        else if(!tag1 || !tag2) return 0;

        if(tag1->time != tag2->time) return 0;

        if(!mpdTagStringsAreEqual(tag1->artist, tag2->artist)) return 0;
        if(!mpdTagStringsAreEqual(tag1->album, tag2->album)) return 0;
        if(!mpdTagStringsAreEqual(tag1->track, tag2->track)) return 0;
        if(!mpdTagStringsAreEqual(tag1->title, tag2->title)) return 0;
        if(!mpdTagStringsAreEqual(tag1->name, tag2->name)) return 0;

        return 1;
}
