// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SCAN_VORBIS_COMMENT_HXX
#define MPD_SCAN_VORBIS_COMMENT_HXX

#include "tag/VorbisComment.hxx"
#include "tag/Handler.hxx"
#include "tag/Type.hxx"

#include <string_view>

class TagHandler;

void
ScanVorbisComment(std::string_view comment, TagHandler &handler) noexcept;

template<typename F>
void
ScanVorbisLabels(F &&for_each, TagHandler &handler) noexcept
{
	using namespace std::string_view_literals;

	if (!handler.WantTag())
		return;

	for (const auto name : {"LABEL"sv, "PUBLISHER"sv, "ORGANIZATION"sv}) {
		bool found = false;
		for_each([&](std::string_view comment) {
			const auto value = GetVorbisCommentValue(comment, name);
			if (!value.empty()) {
				handler.OnTag(TAG_LABEL, value);
				found = true;
			}
		});
		if (found)
			break;
	}
}

#endif
