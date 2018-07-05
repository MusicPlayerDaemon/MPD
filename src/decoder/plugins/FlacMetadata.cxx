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
#include "FlacMetadata.hxx"
#include "lib/xiph/XiphTags.hxx"
#include "MixRampInfo.hxx"
#include "tag/Handler.hxx"
#include "tag/Table.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGain.hxx"
#include "tag/MixRamp.hxx"
#include "ReplayGainInfo.hxx"
#include "util/DivideString.hxx"

bool
flac_parse_replay_gain(ReplayGainInfo &rgi,
		       const FLAC__StreamMetadata_VorbisComment &vc)
{
	rgi.Clear();

	bool found = false;

	const auto *comments = vc.comments;
	for (FLAC__uint32 i = 0, n = vc.num_comments; i < n; ++i)
		if (ParseReplayGainVorbis(rgi,
					  (const char *)comments[i].entry))
			found = true;

	return found;
}

MixRampInfo
flac_parse_mixramp(const FLAC__StreamMetadata_VorbisComment &vc)
{
	MixRampInfo mix_ramp;

	const auto *comments = vc.comments;
	for (FLAC__uint32 i = 0, n = vc.num_comments; i < n; ++i)
		ParseMixRampVorbis(mix_ramp,
				   (const char *)comments[i].entry);

	return mix_ramp;
}

/**
 * Checks if the specified name matches the entry's name, and if yes,
 * returns the comment value;
 */
static const char *
flac_comment_value(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		   const char *name) noexcept
{
	return vorbis_comment_value((const char *)entry->entry, name);
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
flac_copy_comment(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  const char *name, TagType tag_type,
		  TagHandler &handler) noexcept
{
	const char *value = flac_comment_value(entry, name);
	if (value != nullptr) {
		handler.OnTag(tag_type, value);
		return true;
	}

	return false;
}

static void
flac_scan_comment(const FLAC__StreamMetadata_VorbisComment_Entry *entry,
		  TagHandler &handler) noexcept
{
	if (handler.WantPair()) {
		const char *comment = (const char *)entry->entry;
		const DivideString split(comment, '=');
		if (split.IsDefined() && !split.empty())
			handler.OnPair(split.GetFirst(), split.GetSecond());
	}

	for (const struct tag_table *i = xiph_tags; i->name != nullptr; ++i)
		if (flac_copy_comment(entry, i->name, i->type, handler))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (flac_copy_comment(entry,
				      tag_item_names[i], (TagType)i,
				      handler))
			return;
}

static void
flac_scan_comments(const FLAC__StreamMetadata_VorbisComment *comment,
		   TagHandler &handler) noexcept
{
	for (unsigned i = 0; i < comment->num_comments; ++i)
		flac_scan_comment(&comment->comments[i],
				  handler);
}

gcc_pure
static inline SongTime
flac_duration(const FLAC__StreamMetadata_StreamInfo *stream_info) noexcept
{
	assert(stream_info->sample_rate > 0);

	return SongTime::FromScale<uint64_t>(stream_info->total_samples,
					     stream_info->sample_rate);
}

void
flac_scan_metadata(const FLAC__StreamMetadata *block,
		   TagHandler &handler) noexcept
{
	switch (block->type) {
	case FLAC__METADATA_TYPE_VORBIS_COMMENT:
		flac_scan_comments(&block->data.vorbis_comment,
				   handler);
		break;

	case FLAC__METADATA_TYPE_STREAMINFO:
		if (block->data.stream_info.sample_rate > 0)
			handler.OnDuration(flac_duration(&block->data.stream_info));
		break;

	default:
		break;
	}
}

Tag
flac_vorbis_comments_to_tag(const FLAC__StreamMetadata_VorbisComment *comment)
{
	TagBuilder tag_builder;
	AddTagHandler h(tag_builder);
	flac_scan_comments(comment, h);
	return tag_builder.Commit();
}

void
FlacMetadataChain::Scan(TagHandler &handler) noexcept
{
	FLACMetadataIterator iterator(*this);

	do {
		FLAC__StreamMetadata *block = iterator.GetBlock();
		if (block == nullptr)
			break;

		flac_scan_metadata(block, handler);
	} while (iterator.Next());
}
