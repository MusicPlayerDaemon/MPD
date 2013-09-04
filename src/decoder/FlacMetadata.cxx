/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "FlacMetadata.hxx"
#include "XiphTags.hxx"
#include "Tag.hxx"
#include "TagHandler.hxx"
#include "tag/TagTable.hxx"
#include "replay_gain_info.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

static bool
flac_find_float_comment(const FLAC__StreamMetadata *block,
			const char *cmnt, float *fl)
{
	int offset;
	size_t pos;
	int len;
	unsigned char tmp, *p;

	offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block, 0,
								     cmnt);
	if (offset < 0)
		return false;

	pos = strlen(cmnt) + 1; /* 1 is for '=' */
	len = block->data.vorbis_comment.comments[offset].length - pos;
	if (len <= 0)
		return false;

	p = &block->data.vorbis_comment.comments[offset].entry[pos];
	tmp = p[len];
	p[len] = '\0';
	*fl = (float)atof((char *)p);
	p[len] = tmp;

	return true;
}

bool
flac_parse_replay_gain(struct replay_gain_info *rgi,
		       const FLAC__StreamMetadata *block)
{
	bool found = false;

	replay_gain_info_init(rgi);

	if (flac_find_float_comment(block, "replaygain_album_gain",
				    &rgi->tuples[REPLAY_GAIN_ALBUM].gain))
		found = true;
	if (flac_find_float_comment(block, "replaygain_album_peak",
				    &rgi->tuples[REPLAY_GAIN_ALBUM].peak))
		found = true;
	if (flac_find_float_comment(block, "replaygain_track_gain",
				    &rgi->tuples[REPLAY_GAIN_TRACK].gain))
		found = true;
	if (flac_find_float_comment(block, "replaygain_track_peak",
				    &rgi->tuples[REPLAY_GAIN_TRACK].peak))
		found = true;

	return found;
}

static bool
flac_find_string_comment(const FLAC__StreamMetadata *block,
			 const char *cmnt, char **str)
{
	int offset;
	size_t pos;
	int len;
	const unsigned char *p;

	*str = nullptr;
	offset = FLAC__metadata_object_vorbiscomment_find_entry_from(block, 0,
								     cmnt);
	if (offset < 0)
		return false;

	pos = strlen(cmnt) + 1; /* 1 is for '=' */
	len = block->data.vorbis_comment.comments[offset].length - pos;
	if (len <= 0)
		return false;

	p = &block->data.vorbis_comment.comments[offset].entry[pos];
	*str = g_strndup((const char *)p, len);

	return true;
}

bool
flac_parse_mixramp(char **mixramp_start, char **mixramp_end,
		   const FLAC__StreamMetadata *block)
{
	bool found = false;

	if (flac_find_string_comment(block, "mixramp_start", mixramp_start))
		found = true;
	if (flac_find_string_comment(block, "mixramp_end", mixramp_end))
		found = true;

	return found;
}

/**
 * Checks if the specified name matches the entry's name, and if yes,
 * returns the comment value (not null-temrinated).
 */
static const char *
flac_comment_value(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		   const char *name, size_t *length_r)
{
	size_t name_length = strlen(name);
	const char *comment = (const char*)entry->entry;

	if (entry->length <= name_length ||
	    g_ascii_strncasecmp(comment, name, name_length) != 0)
		return nullptr;

	if (comment[name_length] == '=') {
		*length_r = entry->length - name_length - 1;
		return comment + name_length + 1;
	}

	return nullptr;
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
flac_copy_comment(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  const char *name, enum tag_type tag_type,
		  const struct tag_handler *handler, void *handler_ctx)
{
	const char *value;
	size_t value_length;

	value = flac_comment_value(entry, name, &value_length);
	if (value != nullptr) {
		char *p = g_strndup(value, value_length);
		tag_handler_invoke_tag(handler, handler_ctx, tag_type, p);
		g_free(p);
		return true;
	}

	return false;
}

static void
flac_scan_comment(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  const struct tag_handler *handler, void *handler_ctx)
{
	if (handler->pair != nullptr) {
		char *name = g_strdup((const char*)entry->entry);
		char *value = strchr(name, '=');

		if (value != nullptr && value > name) {
			*value++ = 0;
			tag_handler_invoke_pair(handler, handler_ctx,
						name, value);
		}

		g_free(name);
	}

	for (const struct tag_table *i = xiph_tags; i->name != nullptr; ++i)
		if (flac_copy_comment(entry, i->name, i->type,
				      handler, handler_ctx))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (flac_copy_comment(entry,
				      tag_item_names[i], (enum tag_type)i,
				      handler, handler_ctx))
			return;
}

static void
flac_scan_comments(const FLAC__StreamMetadata_VorbisComment *comment,
		   const struct tag_handler *handler, void *handler_ctx)
{
	for (unsigned i = 0; i < comment->num_comments; ++i)
		flac_scan_comment(&comment->comments[i],
				  handler, handler_ctx);
}

void
flac_scan_metadata(const FLAC__StreamMetadata *block,
		   const struct tag_handler *handler, void *handler_ctx)
{
	switch (block->type) {
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flac_scan_comments(&block->data.vorbis_comment,
				   handler, handler_ctx);
		break;

	case FLAC__METADATA_TYPE_STREAMINFO:
		if (block->data.stream_info.sample_rate > 0)
			tag_handler_invoke_duration(handler, handler_ctx,
						    flac_duration(&block->data.stream_info));
		break;

	default:
		break;
	}
}

void
flac_vorbis_comments_to_tag(Tag &tag,
			    const FLAC__StreamMetadata_VorbisComment *comment)
{
	flac_scan_comments(comment, &add_tag_handler, &tag);
}

void
FlacMetadataChain::Scan(const struct tag_handler *handler, void *handler_ctx)
{
	FLACMetadataIterator iterator(*this);

	do {
		FLAC__StreamMetadata *block = iterator.GetBlock();
		if (block == nullptr)
			break;

		flac_scan_metadata(block, handler, handler_ctx);
	} while (iterator.Next());
}
