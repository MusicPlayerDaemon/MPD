/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "VorbisComments.hxx"
#include "XiphTags.hxx"
#include "tag/Table.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGain.hxx"
#include "ReplayGainInfo.hxx"
#include "util/StringView.hxx"
#include "config.h"

#ifndef HAVE_TREMOR
#include <vorbis/codec.h>
#else
#include <tremor/ivorbiscodec.h>
#endif /* HAVE_TREMOR */

template<typename F>
static void
ForEachUserComment(const vorbis_comment &vc, F &&f)
{
	const char *const*const user_comments = vc.user_comments;
	const int*const comment_lengths = vc.comment_lengths;

	const size_t n = vc.comments;
	for (size_t i = 0; i < n; ++i)
		f(StringView{user_comments[i], size_t(comment_lengths[i])});
}

bool
VorbisCommentToReplayGain(ReplayGainInfo &rgi,
			  const vorbis_comment &vc) noexcept
{
	rgi.Clear();

	bool found = false;

	ForEachUserComment(vc, [&](StringView s){
		if (ParseReplayGainVorbis(rgi, s.data))
			found = true;
	});

	return found;
}

/**
 * Check if the comment's name equals the passed name, and if so, copy
 * the comment value into the tag.
 */
static bool
vorbis_copy_comment(StringView comment,
		    StringView name, TagType tag_type,
		    TagHandler &handler) noexcept
{
	const auto value = GetVorbisCommentValue(comment, name);
	if (!value.IsNull()) {
		handler.OnTag(tag_type, value);
		return true;
	}

	return false;
}

static void
vorbis_scan_comment(StringView comment, TagHandler &handler) noexcept
{
	if (handler.WantPair()) {
		const auto split = comment.Split('=');
		if (!split.first.empty() && !split.second.IsNull())
			handler.OnPair(split.first, split.second);
	}

	for (const struct tag_table *i = xiph_tags; i->name != nullptr; ++i)
		if (vorbis_copy_comment(comment, i->name, i->type,
					handler))
			return;

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (vorbis_copy_comment(comment,
					tag_item_names[i], TagType(i),
					handler))
			return;
}

void
VorbisCommentScan(const vorbis_comment &vc, TagHandler &handler) noexcept
{
	ForEachUserComment(vc, [&](StringView s){
		vorbis_scan_comment(s, handler);
	});
}

std::unique_ptr<Tag>
VorbisCommentToTag(const vorbis_comment &vc) noexcept
{
	TagBuilder tag_builder;
	AddTagHandler h(tag_builder);
	VorbisCommentScan(vc, h);
	return tag_builder.empty()
		? nullptr
		: tag_builder.CommitNew();
}
