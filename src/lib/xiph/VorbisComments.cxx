// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VorbisComments.hxx"
#include "VorbisPicture.hxx"
#include "ScanVorbisComment.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "tag/Tag.hxx"
#include "tag/VorbisComment.hxx"
#include "tag/ReplayGainInfo.hxx"
#include "tag/ReplayGainParser.hxx"
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
		f(std::string_view{user_comments[i], size_t(comment_lengths[i])});
}

bool
VorbisCommentToReplayGain(ReplayGainInfo &rgi,
			  const vorbis_comment &vc) noexcept
{
	rgi.Clear();

	bool found = false;

	ForEachUserComment(vc, [&](std::string_view s){
		if (ParseReplayGainVorbis(rgi, s))
			found = true;
	});

	return found;
}

static void
vorbis_scan_comment(std::string_view comment, TagHandler &handler) noexcept
{
	const auto picture_b64 = handler.WantPicture()
		? GetVorbisCommentValue(comment, "METADATA_BLOCK_PICTURE")
		: std::string_view{};
	if (picture_b64.data() != nullptr)
		return ScanVorbisPicture(picture_b64, handler);

	ScanVorbisComment(comment, handler);
}

void
VorbisCommentScan(const vorbis_comment &vc, TagHandler &handler) noexcept
{
	ForEachUserComment(vc, [&](std::string_view s){
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
