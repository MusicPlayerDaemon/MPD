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
