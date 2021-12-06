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

#include "VorbisComments.hxx"
#include "VorbisPicture.hxx"
#include "ScanVorbisComment.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "tag/ReplayGainParser.hxx"
#include "util/StringView.hxx"
#include "decoder/Features.h"

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

static void
vorbis_scan_comment(StringView comment, TagHandler &handler) noexcept
{
	const auto picture_b64 = handler.WantPicture()
		? GetVorbisCommentValue(comment, "METADATA_BLOCK_PICTURE")
		: nullptr;
	if (!picture_b64.IsNull())
		return ScanVorbisPicture(picture_b64, handler);

	ScanVorbisComment(comment, handler);
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
