/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "Id3Load.hxx"
#include "Id3MusicBrainz.hxx"
#include "TagHandler.hxx"
#include "TagTable.hxx"
#include "TagBuilder.hxx"
#include "util/Alloc.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringUtil.hxx"
#include "Log.hxx"

#include <id3tag.h>

#include <string>
#include <stdexcept>

#include <string.h>
#include <stdlib.h>

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

gcc_pure
static id3_utf8_t *
tag_id3_getstring(const struct id3_frame *frame, unsigned i) noexcept
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
import_id3_string(const id3_ucs4_t *ucs4)
{
	id3_utf8_t *utf8 = id3_ucs4_utf8duplicate(ucs4);
	if (gcc_unlikely(utf8 == nullptr))
		return nullptr;

	AtScopeExit(utf8) { free(utf8); };

	return (id3_utf8_t *)xstrdup(Strip((char *)utf8));
}

/**
 * Import a "Text information frame" (ID3v2.4.0 section 4.2).  It
 * contains 2 fields:
 *
 * - encoding
 * - string list
 */
static void
tag_id3_import_text_frame(const struct id3_frame *frame,
			  TagType type,
			  const TagHandler &handler, void *handler_ctx)
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

		id3_utf8_t *utf8 = import_id3_string(ucs4);
		if (utf8 == nullptr)
			continue;

		AtScopeExit(utf8) { free(utf8); };

		tag_handler_invoke_tag(handler, handler_ctx,
				       type, (const char *)utf8);
	}
}

/**
 * Import all text frames with the specified id (ID3v2.4.0 section
 * 4.2).  This is a wrapper for tag_id3_import_text_frame().
 */
static void
tag_id3_import_text(struct id3_tag *tag, const char *id, TagType type,
		    const TagHandler &handler, void *handler_ctx)
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_text_frame(frame, type,
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
tag_id3_import_comment_frame(const struct id3_frame *frame, TagType type,
			     const TagHandler &handler,
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

	id3_utf8_t *utf8 = import_id3_string(ucs4);
	if (utf8 == nullptr)
		return;

	AtScopeExit(utf8) { free(utf8); };

	tag_handler_invoke_tag(handler, handler_ctx, type, (const char *)utf8);
}

/**
 * Import all comment frames (ID3v2.4.0 section 4.10).  This is a
 * wrapper for tag_id3_import_comment_frame().
 */
static void
tag_id3_import_comment(struct id3_tag *tag, const char *id, TagType type,
		       const TagHandler &handler, void *handler_ctx)
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_comment_frame(frame, type,
					     handler, handler_ctx);
}

/**
 * Parse a TXXX name, and convert it to a TagType enum value.
 * Returns TAG_NUM_OF_ITEM_TYPES if the TXXX name is not understood.
 */
gcc_pure
static TagType
tag_id3_parse_txxx_name(const char *name) noexcept
{

	return tag_table_lookup(musicbrainz_txxx_tags, name);
}

/**
 * Import all known MusicBrainz tags from TXXX frames.
 */
static void
tag_id3_import_musicbrainz(struct id3_tag *id3_tag,
			   const TagHandler &handler,
			   void *handler_ctx)
{
	for (unsigned i = 0;; ++i) {
		const id3_frame *frame = id3_tag_findframe(id3_tag, "TXXX", i);
		if (frame == nullptr)
			break;

		id3_utf8_t *name = tag_id3_getstring(frame, 1);
		if (name == nullptr)
			continue;

		AtScopeExit(name) { free(name); };

		id3_utf8_t *value = tag_id3_getstring(frame, 2);
		if (value == nullptr)
			continue;

		AtScopeExit(value) { free(value); };

		tag_handler_invoke_pair(handler, handler_ctx,
					(const char *)name,
					(const char *)value);

		TagType type = tag_id3_parse_txxx_name((const char*)name);

		if (type != TAG_NUM_OF_ITEM_TYPES)
			tag_handler_invoke_tag(handler, handler_ctx,
					       type, (const char*)value);
	}
}

/**
 * Imports the MusicBrainz TrackId from the UFID tag.
 */
static void
tag_id3_import_ufid(struct id3_tag *id3_tag,
		    const TagHandler &handler, void *handler_ctx)
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
	     const TagHandler &handler, void *handler_ctx)
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
	scan_id3_tag(tag, add_tag_handler, &tag_builder);
	return tag_builder.IsEmpty()
		? nullptr
		: tag_builder.CommitNew();
}

bool
tag_id3_scan(InputStream &is,
	     const TagHandler &handler, void *handler_ctx)
{
	UniqueId3Tag tag;

	try {
		tag = tag_id3_load(is);
		if (!tag)
			return false;
	} catch (const std::runtime_error &e) {
		LogError(e);
		return false;
	}

	scan_id3_tag(tag.get(), handler, handler_ctx);
	return true;
}
