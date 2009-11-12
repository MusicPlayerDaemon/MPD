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
#include "flac_metadata.h"
#include "replay_gain.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

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

struct replay_gain_info *
flac_parse_replay_gain(const FLAC__StreamMetadata *block)
{
	struct replay_gain_info *rgi;
	bool found = false;

	rgi = replay_gain_info_new();

	found = flac_find_float_comment(block, "replaygain_album_gain",
					&rgi->tuples[REPLAY_GAIN_ALBUM].gain) ||
		flac_find_float_comment(block, "replaygain_album_peak",
					&rgi->tuples[REPLAY_GAIN_ALBUM].peak) ||
		flac_find_float_comment(block, "replaygain_track_gain",
					&rgi->tuples[REPLAY_GAIN_TRACK].gain) ||
		flac_find_float_comment(block, "replaygain_track_peak",
					&rgi->tuples[REPLAY_GAIN_TRACK].peak);
	if (!found) {
		replay_gain_info_free(rgi);
		rgi = NULL;
	}

	return rgi;
}

/**
 * Checks if the specified name matches the entry's name, and if yes,
 * returns the comment value (not null-temrinated).
 */
static const char *
flac_comment_value(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		   const char *name, const char *char_tnum, size_t *length_r)
{
	size_t name_length = strlen(name);
	size_t char_tnum_length = 0;
	const char *comment = (const char*)entry->entry;

	if (entry->length <= name_length ||
	    g_ascii_strncasecmp(comment, name, name_length) != 0)
		return NULL;

	if (char_tnum != NULL) {
		char_tnum_length = strlen(char_tnum);
		if (entry->length > name_length + char_tnum_length + 2 &&
		    comment[name_length] == '[' &&
		    g_ascii_strncasecmp(comment + name_length + 1,
					char_tnum, char_tnum_length) == 0 &&
		    comment[name_length + char_tnum_length + 1] == ']')
			name_length = name_length + char_tnum_length + 2;
		else if (entry->length > name_length + char_tnum_length &&
			 g_ascii_strncasecmp(comment + name_length,
					     char_tnum, char_tnum_length) == 0)
			name_length = name_length + char_tnum_length;
	}

	if (comment[name_length] == '=') {
		*length_r = entry->length - name_length - 1;
		return comment + name_length + 1;
	}

	return NULL;
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
flac_copy_comment(struct tag *tag,
		  const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  const char *name, enum tag_type tag_type,
		  const char *char_tnum)
{
	const char *value;
	size_t value_length;

	value = flac_comment_value(entry, name, char_tnum, &value_length);
	if (value != NULL) {
		tag_add_item_n(tag, tag_type, value, value_length);
		return true;
	}

	return false;
}

/* tracknumber is used in VCs, MPD uses "track" ..., all the other
 * tag names match */
static const char *VORBIS_COMMENT_TRACK_KEY = "tracknumber";
static const char *VORBIS_COMMENT_DISC_KEY = "discnumber";

static void
flac_parse_comment(struct tag *tag, const char *char_tnum,
		   const FLAC__StreamMetadata_VorbisComment_Entry *entry)
{
	assert(tag != NULL);

	if (flac_copy_comment(tag, entry, VORBIS_COMMENT_TRACK_KEY,
			      TAG_TRACK, char_tnum) ||
	    flac_copy_comment(tag, entry, VORBIS_COMMENT_DISC_KEY,
			      TAG_DISC, char_tnum) ||
	    flac_copy_comment(tag, entry, "album artist",
			      TAG_ALBUM_ARTIST, char_tnum))
		return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (flac_copy_comment(tag, entry,
				      tag_item_names[i], i, char_tnum))
			return;
}

void
flac_vorbis_comments_to_tag(struct tag *tag, const char *char_tnum,
			    const FLAC__StreamMetadata_VorbisComment *comment)
{
	for (unsigned i = 0; i < comment->num_comments; ++i)
		flac_parse_comment(tag, char_tnum, &comment->comments[i]);
}

void
flac_tag_apply_metadata(struct tag *tag, const char *track,
			const FLAC__StreamMetadata *block)
{
	switch (block->type) {
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flac_vorbis_comments_to_tag(tag, track,
					    &block->data.vorbis_comment);
		break;

	case FLAC__METADATA_TYPE_STREAMINFO:
		tag->time = flac_duration(&block->data.stream_info);
		break;

	default:
		break;
	}
}
