// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
