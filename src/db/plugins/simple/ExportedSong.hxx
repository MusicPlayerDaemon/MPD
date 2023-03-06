// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_SIMPLE_EXPORTED_SONG_HXX
#define MPD_DB_SIMPLE_EXPORTED_SONG_HXX

#include "song/LightSong.hxx"
#include "tag/Tag.hxx"

/**
 * The return type for Song::Export().  In addition to implementing
 * #LightSong, it hold allocations necessary to represent the #Song as
 * a #LightSong, e.g. a merged #Tag.
 */
class ExportedSong : public LightSong {
	/**
	 * A reference target for LightSong::tag, but it is only used
	 * if this instance "owns" the #Tag.  For instances referring
	 * to a foreign #Tag instance (e.g. a Song::tag), this field
	 * is not used (and empty).
	 */
	Tag tag_buffer;

public:
	using LightSong::LightSong;

	ExportedSong(const char *_uri, Tag &&_tag) noexcept
		:LightSong(_uri, tag_buffer),
		 tag_buffer(std::move(_tag)) {}

	/* this custom move constructor is necessary so LightSong::tag
	   points to this instance's #Tag field instead of leaving a
	   dangling reference to the source object's #Tag field */
	ExportedSong(ExportedSong &&src) noexcept
		:LightSong(src,
			   /* refer to tag_buffer only if the
			      moved-from instance also owned the Tag
			      which its LightSong::tag field refers
			      to */
			   src.OwnsTag() ? tag_buffer : src.tag),
		 tag_buffer(std::move(src.tag_buffer)) {}

	ExportedSong &operator=(ExportedSong &&) = delete;

private:
	bool OwnsTag() const noexcept {
		return &tag == &tag_buffer;
	}
};

#endif
