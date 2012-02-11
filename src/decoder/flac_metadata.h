/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_FLAC_METADATA_H
#define MPD_FLAC_METADATA_H

#include <assert.h>
#include <stdbool.h>
#include <FLAC/metadata.h>

struct tag_handler;
struct tag;
struct replay_gain_info;

static inline unsigned
flac_duration(const FLAC__StreamMetadata_StreamInfo *stream_info)
{
	assert(stream_info->sample_rate > 0);

	return (stream_info->total_samples + stream_info->sample_rate - 1) /
		stream_info->sample_rate;
}

bool
flac_parse_replay_gain(struct replay_gain_info *rgi,
		       const FLAC__StreamMetadata *block);

bool
flac_parse_mixramp(char **mixramp_start, char **mixramp_end,
		   const FLAC__StreamMetadata *block);

void
flac_vorbis_comments_to_tag(struct tag *tag, const char *char_tnum,
			    const FLAC__StreamMetadata_VorbisComment *comment);

void
flac_scan_metadata(const char *track,
		   const FLAC__StreamMetadata *block,
		   const struct tag_handler *handler, void *handler_ctx);

bool
flac_scan_file2(const char *file, const char *char_tnum,
		const struct tag_handler *handler, void *handler_ctx);

struct tag *
flac_tag_load(const char *file, const char *char_tnum);

#endif
