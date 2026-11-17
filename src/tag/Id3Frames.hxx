// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "util/StringAPI.hxx"

#include <id3tag.h>

#include <ranges>
#include <span>

/**
 * Returns a range of all frames with the specified id.
 *
 * Unlike calling id3_tag_findframe() with increasing indexes (which
 * is O(n^2)), this walks the frame list only once.
 *
 * @param id an ID3v2.4 frame id (this function does not translate
 * obsolete IDs like id3_tag_findframe() does, e.g. "TORY")
 */
[[gnu::pure]]
inline auto
Id3FramesById(const struct id3_tag &tag, const char *id) noexcept
{
	return std::span{tag.frames, tag.nframes} |
		std::views::filter([id](const struct id3_frame *frame){
			return StringIsEqual(frame->id, id);
		});
}
