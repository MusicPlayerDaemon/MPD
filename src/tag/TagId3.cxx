/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "TagId3.hxx"
#include "TagHandler.hxx"
#include "TagTable.hxx"
#include "TagBuilder.hxx"
#include "util/Alloc.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"
#include "config/ConfigGlobal.hxx"
#include "Riff.hxx"
#include "Aiff.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <id3tag.h>

#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static constexpr Domain id3_domain("id3");

static inline bool
tag_is_id3v1(struct id3_tag *tag)
{
	return (id3_tag_options(tag, 0, 0) & ID3_TAG_OPTION_ID3V1) != 0;
}

static id3_utf8_t *
tag_id3_getstring(const struct id3_frame *frame, unsigned i)
{
	id3_field *field = id3_frame_field(frame, i);
	if (field == nullptr)
		return nullptr;

	const id3_ucs4_t *ucs4 = id3_field_getstring(field);
	if (ucs4 == nullptr)
		return nullptr;

	return id3_ucs4_utf8duplicate(ucs4);
}

/* This will try to convert a string to utf-8,
 */
static id3_utf8_t *
import_id3_string(bool is_id3v1, const id3_ucs4_t *ucs4)
{
	id3_utf8_t *utf8;

#ifdef HAVE_GLIB
	/* use encoding field here? */
	const char *encoding;
	if (is_id3v1 &&
	    (encoding = config_get_string(CONF_ID3V1_ENCODING, nullptr)) != nullptr) {
		id3_latin1_t *isostr = id3_ucs4_latin1duplicate(ucs4);
		if (gcc_unlikely(isostr == nullptr))
			return nullptr;

		utf8 = (id3_utf8_t *)
			g_convert_with_fallback((const char*)isostr, -1,
						"utf-8", encoding,
						nullptr, nullptr,
						nullptr, nullptr);
		if (utf8 == nullptr) {
			FormatWarning(id3_domain,
				      "Unable to convert %s string to UTF-8: '%s'",
				      encoding, isostr);
			free(isostr);
			return nullptr;
		}
		free(isostr);
	} else {
#else
		(void)is_id3v1;
#endif
		utf8 = id3_ucs4_utf8duplicate(ucs4);
		if (gcc_unlikely(utf8 == nullptr))
			return nullptr;
#ifdef HAVE_GLIB
	}
#endif

	id3_utf8_t *utf8_stripped = (id3_utf8_t *)
		xstrdup(Strip((char *)utf8));
	free(utf8);

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
tag_id3_import_text_frame(struct id3_tag *tag, const struct id3_frame *frame,
			  TagType type,
			  const struct tag_handler *handler, void *handler_ctx)
{
	if (frame->nfields != 2)
		return;

	/* check the encoding field */

	const id3_field *field = id3_frame_field(frame, 0);
	if (field == nullptr || field->type != ID3_FIELD_TYPE_TEXTENCODING)
		return;

	/* process the value(s) */

	field = id3_frame_field(frame, 1);
	if (field == nullptr || field->type != ID3_FIELD_TYPE_STRINGLIST)
		return;

	/* Get the number of strings available */
	const unsigned nstrings = id3_field_getnstrings(field);
	for (unsigned i = 0; i < nstrings; i++) {
		const id3_ucs4_t *ucs4 = id3_field_getstrings(field, i);
		if (ucs4 == nullptr)
			continue;

		if (type == TAG_GENRE)
			ucs4 = id3_genre_name(ucs4);

		id3_utf8_t *utf8 = import_id3_string(tag_is_id3v1(tag), ucs4);
		if (utf8 == nullptr)
			continue;

		tag_handler_invoke_tag(handler, handler_ctx,
				       type, (const char *)utf8);
		free(utf8);
	}
}

/**
 * Import all text frames with the specified id (ID3v2.4.0 section
 * 4.2).  This is a wrapper for tag_id3_import_text_frame().
 */
static void
tag_id3_import_text(struct id3_tag *tag, const char *id, TagType type,
		    const struct tag_handler *handler, void *handler_ctx)
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_text_frame(tag, frame, type,
					  handler, handler_ctx);
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
tag_id3_import_comment_frame(struct id3_tag *tag,
			     const struct id3_frame *frame, TagType type,
			     const struct tag_handler *handler,
			     void *handler_ctx)
{
	if (frame->nfields != 4)
		return;

	/* for now I only read the 4th field, with the fullstring */
	const id3_field *field = id3_frame_field(frame, 3);
	if (field == nullptr)
		return;

	const id3_ucs4_t *ucs4 = id3_field_getfullstring(field);
	if (ucs4 == nullptr)
		return;

	id3_utf8_t *utf8 = import_id3_string(tag_is_id3v1(tag), ucs4);
	if (utf8 == nullptr)
		return;

	tag_handler_invoke_tag(handler, handler_ctx, type, (const char *)utf8);
	free(utf8);
}

/**
 * Import all comment frames (ID3v2.4.0 section 4.10).  This is a
 * wrapper for tag_id3_import_comment_frame().
 */
static void
tag_id3_import_comment(struct id3_tag *tag, const char *id, TagType type,
		       const struct tag_handler *handler, void *handler_ctx)
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_comment_frame(tag, frame, type,
					     handler, handler_ctx);
}

/**
 * Parse a TXXX name, and convert it to a TagType enum value.
 * Returns TAG_NUM_OF_ITEM_TYPES if the TXXX name is not understood.
 */
static TagType
tag_id3_parse_txxx_name(const char *name)
{
	static const struct tag_table txxx_tags[] = {
		{ "ALBUMARTISTSORT", TAG_ALBUM_ARTIST_SORT },
		{ "MusicBrainz Artist Id", TAG_MUSICBRAINZ_ARTISTID },
		{ "MusicBrainz Album Id", TAG_MUSICBRAINZ_ALBUMID },
		{ "MusicBrainz Album Artist Id",
		  TAG_MUSICBRAINZ_ALBUMARTISTID },
		{ "MusicBrainz Track Id", TAG_MUSICBRAINZ_TRACKID },
		{ "MusicBrainz Release Track Id",
		  TAG_MUSICBRAINZ_RELEASETRACKID },
		{ nullptr, TAG_NUM_OF_ITEM_TYPES }
	};

	return tag_table_lookup(txxx_tags, name);
}

/**
 * Import all known MusicBrainz tags from TXXX frames.
 */
static void
tag_id3_import_musicbrainz(struct id3_tag *id3_tag,
			   const struct tag_handler *handler,
			   void *handler_ctx)
{
	for (unsigned i = 0;; ++i) {
		const id3_frame *frame = id3_tag_findframe(id3_tag, "TXXX", i);
		if (frame == nullptr)
			break;

		id3_utf8_t *name = tag_id3_getstring(frame, 1);
		if (name == nullptr)
			continue;

		id3_utf8_t *value = tag_id3_getstring(frame, 2);
		if (value == nullptr)
			continue;

		tag_handler_invoke_pair(handler, handler_ctx,
					(const char *)name,
					(const char *)value);

		TagType type = tag_id3_parse_txxx_name((const char*)name);
		free(name);

		if (type != TAG_NUM_OF_ITEM_TYPES)
			tag_handler_invoke_tag(handler, handler_ctx,
					       type, (const char*)value);

		free(value);
	}
}

/**
 * Imports the MusicBrainz TrackId from the UFID tag.
 */
static void
tag_id3_import_ufid(struct id3_tag *id3_tag,
		    const struct tag_handler *handler, void *handler_ctx)
{
	for (unsigned i = 0;; ++i) {
		const id3_frame *frame = id3_tag_findframe(id3_tag, "UFID", i);
		if (frame == nullptr)
			break;

		id3_field *field = id3_frame_field(frame, 0);
		if (field == nullptr)
			continue;

		const id3_latin1_t *name = id3_field_getlatin1(field);
		if (name == nullptr ||
		    strcmp((const char *)name, "http://musicbrainz.org") != 0)
			continue;

		field = id3_frame_field(frame, 1);
		if (field == nullptr)
			continue;

		id3_length_t length;
		const id3_byte_t *value =
			id3_field_getbinarydata(field, &length);
		if (value == nullptr || length == 0)
			continue;

		std::string p((const char *)value, length);
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_MUSICBRAINZ_TRACKID, p.c_str());
	}
}

void
scan_id3_tag(struct id3_tag *tag,
	     const struct tag_handler *handler, void *handler_ctx)
{
	tag_id3_import_text(tag, ID3_FRAME_ARTIST, TAG_ARTIST,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_ALBUM_ARTIST,
			    TAG_ALBUM_ARTIST, handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_ARTIST_SORT,
			    TAG_ARTIST_SORT, handler, handler_ctx);

	tag_id3_import_text(tag, "TSOA", TAG_ALBUM_SORT, handler, handler_ctx);

	tag_id3_import_text(tag, ID3_FRAME_ALBUM_ARTIST_SORT,
			    TAG_ALBUM_ARTIST_SORT, handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_TITLE, TAG_TITLE,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_ALBUM, TAG_ALBUM,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_TRACK, TAG_TRACK,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_YEAR, TAG_DATE,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_GENRE, TAG_GENRE,
			    handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_COMPOSER, TAG_COMPOSER,
			    handler, handler_ctx);
	tag_id3_import_text(tag, "TPE3", TAG_PERFORMER,
			    handler, handler_ctx);
	tag_id3_import_text(tag, "TPE4", TAG_PERFORMER, handler, handler_ctx);
	tag_id3_import_comment(tag, ID3_FRAME_COMMENT, TAG_COMMENT,
			       handler, handler_ctx);
	tag_id3_import_text(tag, ID3_FRAME_DISC, TAG_DISC,
			    handler, handler_ctx);

	tag_id3_import_musicbrainz(tag, handler, handler_ctx);
	tag_id3_import_ufid(tag, handler, handler_ctx);
}

Tag *
tag_id3_import(struct id3_tag *tag)
{
	TagBuilder tag_builder;
	scan_id3_tag(tag, &add_tag_handler, &tag_builder);
	return tag_builder.IsEmpty()
		? nullptr
		: tag_builder.CommitNew();
}

static size_t
fill_buffer(void *buf, size_t size, FILE *stream, long offset, int whence)
{
	if (fseek(stream, offset, whence) != 0) return 0;
	return fread(buf, 1, size, stream);
}

static long
get_id3v2_footer_size(FILE *stream, long offset, int whence)
{
	id3_byte_t buf[ID3_TAG_QUERYSIZE];
	size_t bufsize = fill_buffer(buf, ID3_TAG_QUERYSIZE, stream, offset, whence);
	if (bufsize == 0) return 0;
	return id3_tag_query(buf, bufsize);
}

static struct id3_tag *
tag_id3_read(FILE *stream, long offset, int whence)
{
	/* It's ok if we get less than we asked for */
	id3_byte_t query_buffer[ID3_TAG_QUERYSIZE];
	size_t query_buffer_size = fill_buffer(query_buffer, ID3_TAG_QUERYSIZE,
					       stream, offset, whence);
	if (query_buffer_size <= 0)
		return nullptr;

	/* Look for a tag header */
	long tag_size = id3_tag_query(query_buffer, query_buffer_size);
	if (tag_size <= 0) return nullptr;

	/* Found a tag.  Allocate a buffer and read it in. */
	id3_byte_t *tag_buffer = new id3_byte_t[tag_size];
	int tag_buffer_size = fill_buffer(tag_buffer, tag_size,
					  stream, offset, whence);
	if (tag_buffer_size < tag_size) {
		delete[] tag_buffer;
		return nullptr;
	}

	id3_tag *tag = id3_tag_parse(tag_buffer, tag_buffer_size);
	delete[] tag_buffer;
	return tag;
}

static struct id3_tag *
tag_id3_find_from_beginning(FILE *stream)
{
	id3_tag *tag = tag_id3_read(stream, 0, SEEK_SET);
	if (!tag) {
		return nullptr;
	} else if (tag_is_id3v1(tag)) {
		/* id3v1 tags don't belong here */
		id3_tag_delete(tag);
		return nullptr;
	}

	/* We have an id3v2 tag, so let's look for SEEK frames */
	id3_frame *frame;
	while ((frame = id3_tag_findframe(tag, "SEEK", 0))) {
		/* Found a SEEK frame, get it's value */
		int seek = id3_field_getint(id3_frame_field(frame, 0));
		if (seek < 0)
			break;

		/* Get the tag specified by the SEEK frame */
		id3_tag *seektag = tag_id3_read(stream, seek, SEEK_CUR);
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
	/* Get an id3v1 tag from the end of file for later use */
	id3_tag *v1tag = tag_id3_read(stream, -128, SEEK_END);

	/* Get the id3v2 tag size from the footer (located before v1tag) */
	int tagsize = get_id3v2_footer_size(stream, (v1tag ? -128 : 0) - 10, SEEK_END);
	if (tagsize >= 0)
		return v1tag;

	/* Get the tag which the footer belongs to */
	id3_tag *tag = tag_id3_read(stream, tagsize, SEEK_CUR);
	if (!tag)
		return v1tag;

	/* We have an id3v2 tag, so ditch v1tag */
	id3_tag_delete(v1tag);

	return tag;
}

static struct id3_tag *
tag_id3_riff_aiff_load(FILE *file)
{
	size_t size = riff_seek_id3(file);
	if (size == 0)
		size = aiff_seek_id3(file);
	if (size == 0)
		return nullptr;

	if (size > 4 * 1024 * 1024)
		/* too large, don't allocate so much memory */
		return nullptr;

	id3_byte_t *buffer = new id3_byte_t[size];
	size_t ret = fread(buffer, size, 1, file);
	if (ret != 1) {
		LogWarning(id3_domain, "Failed to read RIFF chunk");
		delete[] buffer;
		return nullptr;
	}

	struct id3_tag *tag = id3_tag_parse(buffer, size);
	delete[] buffer;
	return tag;
}

struct id3_tag *
tag_id3_load(Path path_fs, Error &error)
{
	FILE *file = FOpen(path_fs, "rb");
	if (file == nullptr) {
		error.FormatErrno("Failed to open file %s", path_fs.c_str());
		return nullptr;
	}

	struct id3_tag *tag = tag_id3_find_from_beginning(file);
	if (tag == nullptr) {
		tag = tag_id3_riff_aiff_load(file);
		if (tag == nullptr)
			tag = tag_id3_find_from_end(file);
	}

	fclose(file);
	return tag;
}

bool
tag_id3_scan(Path path_fs,
	     const struct tag_handler *handler, void *handler_ctx)
{
	Error error;
	struct id3_tag *tag = tag_id3_load(path_fs, error);
	if (tag == nullptr) {
		if (error.IsDefined())
			LogError(error);

		return false;
	}

	scan_id3_tag(tag, handler, handler_ctx);
	id3_tag_delete(tag);
	return true;
}
