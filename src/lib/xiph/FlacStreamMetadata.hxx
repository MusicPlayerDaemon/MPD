/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_FLAC_STREAM_METADATA_HXX
#define MPD_FLAC_STREAM_METADATA_HXX

#include <FLAC/format.h>

class TagHandler;
class MixRampInfo;

struct Tag;
struct ReplayGainInfo;

bool
flac_parse_replay_gain(ReplayGainInfo &rgi,
		       const FLAC__StreamMetadata_VorbisComment &vc);

MixRampInfo
flac_parse_mixramp(const FLAC__StreamMetadata_VorbisComment &vc);

Tag
flac_vorbis_comments_to_tag(const FLAC__StreamMetadata_VorbisComment *comment);

void
flac_scan_metadata(const FLAC__StreamMetadata *block,
		   TagHandler &handler) noexcept;

#endif
