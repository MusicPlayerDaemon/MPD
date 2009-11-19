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

#  ifndef ID3_FRAME_COMPOSER
#    define ID3_FRAME_COMPOSER "TCOM"
#  endif
#  ifndef ID3_FRAME_DISC
#    define ID3_FRAME_DISC "TPOS"
#  endif

#ifndef ID3_FRAME_ARTIST_SORT
#define ID3_FRAME_ARTIST_SORT "TSOP"
#endif

#ifndef ID3_FRAME_ALBUM_ARTIST_SORT
#define ID3_FRAME_ALBUM_ARTIST_SORT "TSO2" /* this one is unofficial, introduced by Itunes */
#endif

#ifndef ID3_FRAME_ALBUM_ARTIST
#define ID3_FRAME_ALBUM_ARTIST "TPE2"
#endif

static inline bool
tag_is_id3v1(struct id3_tag *tag)
{
	return (id3_tag_options(tag, 0, 0) & ID3_TAG_OPTION_ID3V1) != 0;
}

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
static id3_utf8_t *
import_id3_string(bool is_id3v1, const id3_ucs4_t *ucs4)
{
	id3_utf8_t *utf8, *utf8_stripped;
	id3_latin1_t *isostr;
	const char *encoding;

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

/**
 * Import a "Text information frame" (ID3v2.4.0 section 4.2).  It
 * contains 2 fields:
 *
 * - encoding
 * - string list
 */
static void
tag_id3_import_text(struct tag *dest, struct id3_tag *tag, const char *id,
		    enum tag_type type)
{
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_utf8_t *utf8;
	union id3_field const *field;
	unsigned int nstrings, i;

	frame = id3_tag_findframe(tag, id, 0);
	if (frame == NULL || frame->nfields != 2)
		return;

	/* check the encoding field */

	field = id3_frame_field(frame, 0);
	if (field == NULL || field->type != ID3_FIELD_TYPE_TEXTENCODING)
		return;

	/* process the value(s) */

	field = id3_frame_field(frame, 1);
	if (field == NULL || field->type != ID3_FIELD_TYPE_STRINGLIST)
		return;

	/* Get the number of strings available */
	nstrings = id3_field_getnstrings(field);
	for (i = 0; i < nstrings; i++) {
		ucs4 = id3_field_getstrings(field, i);
		if (ucs4 == NULL)
			continue;

		if (type == TAG_GENRE)
			ucs4 = id3_genre_name(ucs4);

		utf8 = import_id3_string(tag_is_id3v1(tag), ucs4);
		if (utf8 == NULL)
			continue;

		tag_add_item(dest, type, (char *)utf8);
		g_free(utf8);
	}
}

/**
 * Import a "Comment frame" (ID3v2.4.0 section 4.10).  It
 * contains 4 fields:
 *
 * - encoding
 * - language
 * - string
 * - full string (we use this one)
 */
static void
tag_id3_import_comment(struct tag *dest, struct id3_tag *tag, const char *id,
		       enum tag_type type)
{
	struct id3_frame const *frame;
	id3_ucs4_t const *ucs4;
	id3_utf8_t *utf8;
	union id3_field const *field;

	frame = id3_tag_findframe(tag, id, 0);
	if (frame == NULL || frame->nfields != 4)
		return;

	/* for now I only read the 4th field, with the fullstring */
	field = id3_frame_field(frame, 3);
	if (field == NULL)
		return;

	ucs4 = id3_field_getfullstring(field);
	if (ucs4 == NULL)
		return;

	utf8 = import_id3_string(tag_is_id3v1(tag), ucs4);
	if (utf8 == NULL)
		return;

	tag_add_item(dest, type, (char *)utf8);
	g_free(utf8);
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
		{ TAG_ALBUM_ARTIST_SORT, "ALBUMARTISTSORT" },
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

	tag_id3_import_text(ret, tag, ID3_FRAME_ARTIST, TAG_ARTIST);
	tag_id3_import_text(ret, tag, ID3_FRAME_ALBUM_ARTIST,
			    TAG_ALBUM_ARTIST);
	tag_id3_import_text(ret, tag, ID3_FRAME_ARTIST_SORT,
			    TAG_ARTIST_SORT);
	tag_id3_import_text(ret, tag, ID3_FRAME_ALBUM_ARTIST_SORT,
			    TAG_ALBUM_ARTIST_SORT);
	tag_id3_import_text(ret, tag, ID3_FRAME_TITLE, TAG_TITLE);
	tag_id3_import_text(ret, tag, ID3_FRAME_ALBUM, TAG_ALBUM);
	tag_id3_import_text(ret, tag, ID3_FRAME_TRACK, TAG_TRACK);
	tag_id3_import_text(ret, tag, ID3_FRAME_YEAR, TAG_DATE);
	tag_id3_import_text(ret, tag, ID3_FRAME_GENRE, TAG_GENRE);
	tag_id3_import_text(ret, tag, ID3_FRAME_COMPOSER, TAG_COMPOSER);
	tag_id3_import_text(ret, tag, "TPE3", TAG_PERFORMER);
	tag_id3_import_text(ret, tag, "TPE4", TAG_PERFORMER);
	tag_id3_import_comment(ret, tag, ID3_FRAME_COMMENT, TAG_COMMENT);
	tag_id3_import_text(ret, tag, ID3_FRAME_DISC, TAG_DISC);

	tag_id3_import_musicbrainz(ret, tag);
	tag_id3_import_ufid(ret, tag);

	if (tag_is_empty(ret)) {
		tag_free(ret);
		ret = NULL;
	}

	return ret;
}

static int
fill_buffer(void *buf, size_t size, FILE *stream, long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0) return 0;
	return fread(buf, 1, size, stream);
}

static int
get_id3v2_footer_size(FILE *stream, long offset, int whence)
{
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	int bufsize;

	bufsize = fill_buffer(buf, ID3_TAG_QUERYSIZE, stream, offset, whence);
	if (bufsize <= 0) return 0;
	return id3_tag_query(buf, bufsize);
}

static struct id3_tag *
tag_id3_read(FILE *stream, long offset, int whence)
{
	struct id3_tag *tag;
	id3_byte_t query_buffer[ID3_TAG_QUERYSIZE];
	id3_byte_t *tag_buffer;
	int tag_size;
	int query_buffer_size;
	int tag_buffer_size;

	/* It's ok if we get less than we asked for */
	query_buffer_size = fill_buffer(query_buffer, ID3_TAG_QUERYSIZE,
				   stream, offset, whence);
	if (query_buffer_size <= 0) return NULL;

	/* Look for a tag header */
	tag_size = id3_tag_query(query_buffer, query_buffer_size);
	if (tag_size <= 0) return NULL;

	/* Found a tag.  Allocate a buffer and read it in. */
	tag_buffer = g_malloc(tag_size);
	if (!tag_buffer) return NULL;

	tag_buffer_size = fill_buffer(tag_buffer, tag_size, stream, offset, whence);
	if (tag_buffer_size < tag_size) {
		g_free(tag_buffer);
		return NULL;
	}

	tag = id3_tag_parse(tag_buffer, tag_buffer_size);

	g_free(tag_buffer);

	return tag;
}

static struct id3_tag *
tag_id3_find_from_beginning(FILE *stream)
{
	struct id3_tag *tag;
	struct id3_tag *seektag;
	struct id3_frame *frame;
	int seek;

	tag = tag_id3_read(stream, 0, SEEK_SET);
	if (!tag) {
		return NULL;
	} else if (tag_is_id3v1(tag)) {
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
		seektag = tag_id3_read(stream, seek, SEEK_CUR);
		if (!seektag || tag_is_id3v1(seektag))
			break;

		/* Replace the old tag with the new one */
		id3_tag_delete(tag);
		tag = seektag;
	}

	return tag;
}

static struct id3_tag *
tag_id3_find_from_end(FILE *stream)
{
	struct id3_tag *tag;
	struct id3_tag *v1tag;
	int tagsize;

	/* Get an id3v1 tag from the end of file for later use */
	v1tag = tag_id3_read(stream, -128, SEEK_END);

	/* Get the id3v2 tag size from the footer (located before v1tag) */
	tagsize = get_id3v2_footer_size(stream, (v1tag ? -128 : 0) - 10, SEEK_END);
	if (tagsize >= 0)
		return v1tag;

	/* Get the tag which the footer belongs to */
	tag = tag_id3_read(stream, tagsize, SEEK_CUR);
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

	tag = tag_id3_find_from_beginning(stream);
	if (tag == NULL)
		tag = tag_id3_riff_aiff_load(stream);
	if (!tag)
		tag = tag_id3_find_from_end(stream);

	fclose(stream);

	if (!tag)
		return NULL;
	ret = tag_id3_import(tag);
	id3_tag_delete(tag);
	return ret;
}
