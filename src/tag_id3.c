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

#include "tag_id3.h"
#include "tag.h"
#include "riff.h"
#include "aiff.h"
#include "conf.h"

#include <glib.h>
#include <id3tag.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "id3"

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

#ifndef ID3_FRAME_ALBUM_ARTIST_SORT
#define ID3_FRAME_ALBUM_ARTIST_SORT "TSO2"
#endif

#ifndef ID3_FRAME_ALBUM_ARTIST
#define ID3_FRAME_ALBUM_ARTIST "TPE2"
#endif

static id3_utf8_t *
tag_id3_getstring(const struct id3_frame *frame, unsigned i)
{
	union id3_field *field;
	const id3_ucs4_t *ucs4;

	field = id3_frame_field(frame, i);
	if (field == NULL)
		return NULL;

	ucs4 = id3_field_getstring(field);
	if (ucs4 == NULL)
		return NULL;

	return id3_ucs4_utf8duplicate(ucs4);
}

/* This will try to convert a string to utf-8,
 */
static id3_utf8_t * processID3FieldString (int is_id3v1, const id3_ucs4_t *ucs4, int type)
{
	id3_utf8_t *utf8, *utf8_stripped;
	id3_latin1_t *isostr;
	const char *encoding;

	if (type == TAG_ITEM_GENRE)
		ucs4 = id3_genre_name(ucs4);
	/* use encoding field here? */
	if (is_id3v1 &&
	    (encoding = config_get_string(CONF_ID3V1_ENCODING, NULL)) != NULL) {
		isostr = id3_ucs4_latin1duplicate(ucs4);
		if (G_UNLIKELY(!isostr)) {
			return NULL;
		}

		utf8 = (id3_utf8_t *)
			g_convert_with_fallback((const char*)isostr, -1,
						encoding, "utf-8",
						NULL, NULL, NULL, NULL);
		if (utf8 == NULL) {
			g_debug("Unable to convert %s string to UTF-8: '%s'",
				encoding, isostr);
			g_free(isostr);
			return NULL;
		}
		g_free(isostr);
	} else {
		utf8 = id3_ucs4_utf8duplicate(ucs4);
		if (G_UNLIKELY(!utf8)) {
			return NULL;
		}
	}

	utf8_stripped = (id3_utf8_t *)g_strdup(g_strstrip((gchar *)utf8));
	g_free(utf8);

	return utf8_stripped;
}

static void
getID3Info(struct id3_tag *tag, const char *id, int type, struct tag *mpdTag)
{
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_utf8_t *utf8;
	union id3_field const *field;
	unsigned int nstrings, i;

	frame = id3_tag_findframe(tag, id, 0);
	/* Check frame */
	if (!frame)
	{
		return;
	}
	/* Check fields in frame */
	if(frame->nfields == 0)
	{
		g_debug("Frame has no fields");
		return;
	}

	/* Starting with T is a stringlist */
	if (id[0] == 'T')
	{
		/* This one contains 2 fields:
		 * 1st: Text encoding
		 * 2: Stringlist
		 * Shamefully this isn't the RL case.
		 * But I am going to enforce it anyway. 
		 */
		if(frame->nfields != 2) 
		{
			g_debug("Invalid number '%i' of fields for TXX frame",
				frame->nfields);
			return;
		}
		field = &frame->fields[0];
		/**
		 * First field is encoding field.
		 * This is ignored by mpd.
		 */
		if(field->type != ID3_FIELD_TYPE_TEXTENCODING)
		{
			g_debug("Expected encoding, found: %i",
				field->type);
		}
		/* Process remaining fields, should be only one */
		field = &frame->fields[1];
		/* Encoding field */
		if(field->type == ID3_FIELD_TYPE_STRINGLIST) {
			/* Get the number of strings available */
			nstrings = id3_field_getnstrings(field);
			for (i = 0; i < nstrings; i++) {
				ucs4 = id3_field_getstrings(field,i);
				if(!ucs4)
					continue;
				utf8 = processID3FieldString(isId3v1(tag),ucs4, type);
				if(!utf8)
					continue;

				tag_add_item(mpdTag, type, (char *)utf8);
				g_free(utf8);
			}
		}
		else {
			g_warning("Field type not processed: %i",
				  (int)id3_field_gettextencoding(field));
		}
	}
	/* A comment frame */
	else if(!strcmp(ID3_FRAME_COMMENT, id))
	{
		/* A comment frame is different... */
	/* 1st: encoding
         * 2nd: Language
         * 3rd: String
         * 4th: FullString.
         * The 'value' we want is in the 4th field
         */
		if(frame->nfields == 4)
		{
			/* for now I only read the 4th field, with the fullstring */
			field = &frame->fields[3];
			if(field->type == ID3_FIELD_TYPE_STRINGFULL)
			{
				ucs4 = id3_field_getfullstring(field);
				if(ucs4)
				{
					utf8 = processID3FieldString(isId3v1(tag),ucs4, type);
					if(utf8)
					{
						tag_add_item(mpdTag, type, (char *)utf8);
						g_free(utf8);
					}
				}
			}
			else
			{
				g_debug("4th field in comment frame differs from expected, got '%i': ignoring",
					field->type);
			}
		}
		else
		{
			g_debug("Invalid 'comments' tag, got '%i' fields instead of 4",
				frame->nfields);
		}
	}
	/* Unsupported */
	else
		g_debug("Unsupported tag type requrested");
}

/**
 * Parse a TXXX name, and convert it to a tag_type enum value.
 * Returns TAG_NUM_OF_ITEM_TYPES if the TXXX name is not understood.
 */
static enum tag_type
tag_id3_parse_txxx_name(const char *name)
{
	static const struct {
		enum tag_type type;
		const char *name;
	} musicbrainz_txxx[] = {
		{ TAG_MUSICBRAINZ_ARTISTID, "MusicBrainz Artist Id" },
		{ TAG_MUSICBRAINZ_ALBUMID, "MusicBrainz Album Id" },
		{ TAG_MUSICBRAINZ_ALBUMARTISTID,
		  "MusicBrainz Album Artist Id" },
		{ TAG_MUSICBRAINZ_TRACKID, "MusicBrainz Track Id" },
	};

	for (unsigned i = 0; i < G_N_ELEMENTS(musicbrainz_txxx); ++i)
		if (strcmp(name, musicbrainz_txxx[i].name) == 0)
			return musicbrainz_txxx[i].type;

	return TAG_NUM_OF_ITEM_TYPES;
}

/**
 * Import all known MusicBrainz tags from TXXX frames.
 */
static void
tag_id3_import_musicbrainz(struct tag *mpd_tag, struct id3_tag *id3_tag)
{
	for (unsigned i = 0;; ++i) {
		const struct id3_frame *frame;
		id3_utf8_t *name, *value;
		enum tag_type type;

		frame = id3_tag_findframe(id3_tag, "TXXX", i);
		if (frame == NULL)
			break;

		name = tag_id3_getstring(frame, 1);
		if (name == NULL)
			continue;

		type = tag_id3_parse_txxx_name((const char*)name);
		free(name);

		if (type == TAG_NUM_OF_ITEM_TYPES)
			continue;

		value = tag_id3_getstring(frame, 2);
		if (value == NULL)
			continue;

		tag_add_item(mpd_tag, type, (const char*)value);
		free(value);
	}
}

/**
 * Imports the MusicBrainz TrackId from the UFID tag.
 */
static void
tag_id3_import_ufid(struct tag *mpd_tag, struct id3_tag *id3_tag)
{
	for (unsigned i = 0;; ++i) {
		const struct id3_frame *frame;
		union id3_field *field;
		const id3_latin1_t *name;
		const id3_byte_t *value;
		id3_length_t length;

		frame = id3_tag_findframe(id3_tag, "UFID", i);
		if (frame == NULL)
			break;

		field = id3_frame_field(frame, 0);
		if (field == NULL)
			continue;

		name = id3_field_getlatin1(field);
		if (name == NULL ||
		    strcmp((const char *)name, "http://musicbrainz.org") != 0)
			continue;

		field = id3_frame_field(frame, 1);
		if (field == NULL)
			continue;

		value = id3_field_getbinarydata(field, &length);
		if (value == NULL || length == 0)
			continue;

		tag_add_item_n(mpd_tag, TAG_MUSICBRAINZ_TRACKID,
			       (const char*)value, length);
	}
}

struct tag *tag_id3_import(struct id3_tag * tag)
{
	struct tag *ret = tag_new();

	getID3Info(tag, ID3_FRAME_ARTIST, TAG_ITEM_ARTIST, ret);
	getID3Info(tag, ID3_FRAME_ALBUM_ARTIST,
		   TAG_ITEM_ALBUM_ARTIST, ret);
	getID3Info(tag, ID3_FRAME_ALBUM_ARTIST_SORT,
		   TAG_ITEM_ALBUM_ARTIST, ret);
	getID3Info(tag, ID3_FRAME_TITLE, TAG_ITEM_TITLE, ret);
	getID3Info(tag, ID3_FRAME_ALBUM, TAG_ITEM_ALBUM, ret);
	getID3Info(tag, ID3_FRAME_TRACK, TAG_ITEM_TRACK, ret);
	getID3Info(tag, ID3_FRAME_YEAR, TAG_ITEM_DATE, ret);
	getID3Info(tag, ID3_FRAME_GENRE, TAG_ITEM_GENRE, ret);
	getID3Info(tag, ID3_FRAME_COMPOSER, TAG_ITEM_COMPOSER, ret);
	getID3Info(tag, ID3_FRAME_PERFORMER, TAG_ITEM_PERFORMER, ret);
	getID3Info(tag, ID3_FRAME_COMMENT, TAG_ITEM_COMMENT, ret);
	getID3Info(tag, ID3_FRAME_DISC, TAG_ITEM_DISC, ret);

	tag_id3_import_musicbrainz(ret, tag);
	tag_id3_import_ufid(ret, tag);

	if (tag_is_empty(ret)) {
		tag_free(ret);
		ret = NULL;
	}

	return ret;
}

static int fillBuffer(void *buf, size_t size, FILE * stream,
		      long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0) return 0;
	return fread(buf, 1, size, stream);
}

static int getId3v2FooterSize(FILE * stream, long offset, int whence)
{
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	int bufsize;

	bufsize = fillBuffer(buf, ID3_TAG_QUERYSIZE, stream, offset, whence);
	if (bufsize <= 0) return 0;
	return id3_tag_query(buf, bufsize);
}

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
	tagBuf = g_malloc(tagSize);
	if (!tagBuf) return NULL;

	tagBufSize = fillBuffer(tagBuf, tagSize, stream, offset, whence);
	if (tagBufSize < tagSize) {
		g_free(tagBuf);
		return NULL;
	}

	tag = id3_tag_parse(tagBuf, tagBufSize);

	g_free(tagBuf);

	return tag;
}

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

static struct id3_tag *
tag_id3_riff_aiff_load(FILE *file)
{
	size_t size;
	void *buffer;
	size_t ret;
	struct id3_tag *tag;

	size = riff_seek_id3(file);
	if (size == 0)
		size = aiff_seek_id3(file);
	if (size == 0)
		return NULL;

	if (size > 4 * 1024 * 1024)
		/* too large, don't allocate so much memory */
		return NULL;

	buffer = g_malloc(size);
	ret = fread(buffer, size, 1, file);
	if (ret != 1) {
		g_warning("Failed to read RIFF chunk");
		g_free(buffer);
		return NULL;
	}

	tag = id3_tag_parse(buffer, size);
	g_free(buffer);
	return tag;
}

struct tag *tag_id3_load(const char *file)
{
	struct tag *ret;
	struct id3_tag *tag;
	FILE *stream;

	stream = fopen(file, "r");
	if (!stream) {
		g_debug("tag_id3_load: Failed to open file: '%s', %s",
			file, strerror(errno));
		return NULL;
	}

	tag = findId3TagFromBeginning(stream);
	if (tag == NULL)
		tag = tag_id3_riff_aiff_load(stream);
	if (!tag)
		tag = findId3TagFromEnd(stream);

	fclose(stream);

	if (!tag)
		return NULL;
	ret = tag_id3_import(tag);
	id3_tag_delete(tag);
	return ret;
}
