// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "song/LightSong.hxx"
#include "tag/WithTagBuffer.hxx"

/**
 * The return type for Song::Export().  In addition to implementing
 * #LightSong, it hold allocations necessary to represent the #Song as
 * a #LightSong, e.g. a merged #Tag.
 */
class ExportedSong : WithTagBuffer, public LightSong {
public:
	using LightSong::LightSong;

	ExportedSong(const char *_uri, Tag &&_tag) noexcept
		:WithTagBuffer(std::move(_tag)),
		 LightSong(_uri, tag_buffer) {}

	/* this custom move constructor is necessary so LightSong::tag
	   points to this instance's #Tag field instead of leaving a
	   dangling reference to the source object's #Tag field */
	ExportedSong(ExportedSong &&src) noexcept
		:WithTagBuffer(std::move(src.tag_buffer)),
		 LightSong(src,
			   /* refer to tag_buffer only if the
			      moved-from instance also owned the Tag
			      which its LightSong::tag field refers
			      to */
			   src.OwnsTag() ? tag_buffer : src.tag) {}

	ExportedSong &operator=(ExportedSong &&) = delete;

private:
	bool OwnsTag() const noexcept {
		return &tag == &tag_buffer;
	}
};
