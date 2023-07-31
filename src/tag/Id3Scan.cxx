// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Id3Scan.hxx"
#include "Id3String.hxx"
#include "Id3Load.hxx"
#include "Handler.hxx"
#include "Table.hxx"
#include "Builder.hxx"
#include "Tag.hxx"
#include "Id3MusicBrainz.hxx"
#include "util/StringAPI.hxx"
#include "util/StringStrip.hxx"

#include <id3tag.h>

#include <cassert>

#include <string.h>
#include <stdlib.h>

#ifndef ID3_FRAME_COMPOSER
#define ID3_FRAME_COMPOSER "TCOM"
#endif

#ifndef ID3_FRAME_DISC
#define ID3_FRAME_DISC "TPOS"
#endif

#ifndef ID3_FRAME_ARTIST_SORT
#define ID3_FRAME_ARTIST_SORT "TSOP"
#endif

#ifndef ID3_FRAME_ALBUM_ARTIST_SORT
#define ID3_FRAME_ALBUM_ARTIST_SORT "TSO2" /* this one is unofficial, introduced by Itunes */
#endif

#ifndef ID3_FRAME_ALBUM_ARTIST
#define ID3_FRAME_ALBUM_ARTIST "TPE2"
#endif

#ifndef ID3_FRAME_TITLE_SORT
#define ID3_FRAME_TITLE_SORT "TSOT"
#endif

#ifndef ID3_FRAME_ORIGINAL_RELEASE_DATE
#define ID3_FRAME_ORIGINAL_RELEASE_DATE "TDOR"
#endif

#ifndef ID3_FRAME_LABEL
#define ID3_FRAME_LABEL "TPUB"
#endif

#ifndef ID3_FRAME_MOOD
#define ID3_FRAME_MOOD "TMOO"
#endif

#ifndef ID3_FRAME_LYRICS
#define ID3_FRAME_LYRICS "USLT"
#endif

[[gnu::pure]]
static Id3String
tag_id3_getstring(const struct id3_frame *frame, unsigned i) noexcept
{
	id3_field *field = id3_frame_field(frame, i);
	if (field == nullptr)
		return {};

	const id3_ucs4_t *ucs4 = id3_field_getstring(field);
	if (ucs4 == nullptr)
		return {};

	return Id3String::FromUCS4(ucs4);
}

static void
InvokeOnTag(TagHandler &handler, TagType type, const id3_ucs4_t *ucs4) noexcept
{
	assert(type < TAG_NUM_OF_ITEM_TYPES);
	assert(ucs4 != nullptr);

	const auto utf8 = Id3String::FromUCS4(ucs4);
	if (!utf8)
		return;

	handler.OnTag(type, Strip(std::string_view{utf8.c_str()}));
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
			  TagHandler &handler) noexcept
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

		InvokeOnTag(handler, type, ucs4);
	}
}

/**
 * Import all text frames with the specified id (ID3v2.4.0 section
 * 4.2).  This is a wrapper for tag_id3_import_text_frame().
 */
static void
tag_id3_import_text(const struct id3_tag *tag, const char *id, TagType type,
		    TagHandler &handler) noexcept
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_text_frame(frame, type,
					  handler);
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
			     TagHandler &handler) noexcept
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

	InvokeOnTag(handler, type, ucs4);
}

/**
 * Import all comment frames (ID3v2.4.0 section 4.10).  This is a
 * wrapper for tag_id3_import_comment_frame().
 */
static void
tag_id3_import_comment(const struct id3_tag *tag, const char *id, TagType type,
		       TagHandler &handler) noexcept
{
	const struct id3_frame *frame;
	for (unsigned i = 0;
	     (frame = id3_tag_findframe(tag, id, i)) != nullptr; ++i)
		tag_id3_import_comment_frame(frame, type,
					     handler);
}

/**
 * Parse a TXXX name, and convert it to a TagType enum value.
 * Returns TAG_NUM_OF_ITEM_TYPES if the TXXX name is not understood.
 */
[[gnu::pure]]
static TagType
tag_id3_parse_txxx_name(const char *name) noexcept
{

	return tag_table_lookup(musicbrainz_txxx_tags, name);
}

/**
 * Import all known MusicBrainz tags from TXXX frames.
 */
static void
tag_id3_import_musicbrainz(const struct id3_tag *id3_tag,
			   TagHandler &handler) noexcept
{
	for (unsigned i = 0;; ++i) {
		const id3_frame *frame = id3_tag_findframe(id3_tag, "TXXX", i);
		if (frame == nullptr)
			break;

		const auto name = tag_id3_getstring(frame, 1);
		if (!name)
			continue;

		const auto value = tag_id3_getstring(frame, 2);
		if (!value)
			continue;

		handler.OnPair(name.c_str(), value.c_str());

		TagType type = tag_id3_parse_txxx_name(name.c_str());

		if (type != TAG_NUM_OF_ITEM_TYPES)
			handler.OnTag(type, value.c_str());
	}
}

/**
 * Imports the MusicBrainz TrackId from the UFID tag.
 */
static void
tag_id3_import_ufid(const struct id3_tag *id3_tag,
		    TagHandler &handler) noexcept
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

		handler.OnTag(TAG_MUSICBRAINZ_TRACKID,
			      {(const char *)value, length});
	}
}

/**
 * Handle "APIC" ("attached picture") tags.
 */
static void
tag_id3_handle_apic(const struct id3_tag *id3_tag,
		    TagHandler &handler) noexcept
{
	if (!handler.WantPicture())
		return;

	for (unsigned i = 0;; ++i) {
		const id3_frame *frame = id3_tag_findframe(id3_tag, "APIC", i);
		if (frame == nullptr)
			break;

		id3_field *mime_type_field = id3_frame_field(frame, 1);
		if (mime_type_field == nullptr)
			continue;

		const char *mime_type = (const char *)
			id3_field_getlatin1(mime_type_field);
		if (mime_type != nullptr &&
		    StringIsEqual(mime_type, "-->"))
			/* this is a URL, not image data */
			continue;

		id3_field *data_field = id3_frame_field(frame, 4);
		if (data_field == nullptr ||
		    data_field->type != ID3_FIELD_TYPE_BINARYDATA)
			continue;

		id3_length_t size;
		const id3_byte_t *data =
			id3_field_getbinarydata(data_field, &size);
		if (data == nullptr || size == 0)
			continue;

		handler.OnPicture(mime_type,
				  std::as_bytes(std::span{data, size}));
	}
}

void
scan_id3_tag(const struct id3_tag *tag, TagHandler &handler) noexcept
{
	tag_id3_import_text(tag, ID3_FRAME_ARTIST, TAG_ARTIST,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_ALBUM_ARTIST,
			    TAG_ALBUM_ARTIST, handler);
	tag_id3_import_text(tag, ID3_FRAME_ARTIST_SORT,
			    TAG_ARTIST_SORT, handler);

	tag_id3_import_text(tag, "TSOA", TAG_ALBUM_SORT, handler);

	tag_id3_import_text(tag, ID3_FRAME_ALBUM_ARTIST_SORT,
			    TAG_ALBUM_ARTIST_SORT, handler);
	tag_id3_import_text(tag, ID3_FRAME_TITLE, TAG_TITLE,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_ALBUM, TAG_ALBUM,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_TRACK, TAG_TRACK,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_YEAR, TAG_DATE,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_ORIGINAL_RELEASE_DATE, TAG_ORIGINAL_DATE,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_GENRE, TAG_GENRE,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_COMPOSER, TAG_COMPOSER,
			    handler);
	tag_id3_import_text(tag, "TPE3", TAG_CONDUCTOR,
			    handler);
	tag_id3_import_text(tag, "TPE4", TAG_PERFORMER, handler);
	tag_id3_import_text(tag, "TIT1", TAG_GROUPING, handler);
	tag_id3_import_comment(tag, ID3_FRAME_COMMENT, TAG_COMMENT,
			       handler);
	tag_id3_import_text(tag, ID3_FRAME_DISC, TAG_DISC,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_LABEL, TAG_LABEL,
			    handler);
	tag_id3_import_text(tag, ID3_FRAME_MOOD, TAG_MOOD, handler);
	tag_id3_import_text(tag, ID3_FRAME_TITLE_SORT, TAG_TITLE_SORT, handler);
        tag_id3_import_comment(tag, ID3_FRAME_LYRICS, TAG_LYRICS,
                            handler);

	tag_id3_import_musicbrainz(tag, handler);
	tag_id3_import_ufid(tag, handler);
	tag_id3_handle_apic(tag, handler);
}

Tag
tag_id3_import(const struct id3_tag *tag) noexcept
{
	TagBuilder tag_builder;
	AddTagHandler h(tag_builder);
	scan_id3_tag(tag, h);
	return tag_builder.Commit();
}

bool
tag_id3_scan(InputStream &is, TagHandler &handler)
{
	auto tag = tag_id3_load(is);
	if (!tag)
		return false;

	scan_id3_tag(tag.get(), handler);
	return true;
}
