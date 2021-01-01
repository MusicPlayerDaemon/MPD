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

#ifndef MPD_VORBIS_COMMENT_HXX
#define MPD_VORBIS_COMMENT_HXX

#include <vorbis/codec.h>

/**
 * OO wrapper for a #vorbis_comment instance.
 */
class VorbisComment {
	vorbis_comment vc;

public:
	VorbisComment() noexcept {
		vorbis_comment_init(&vc);
	}

	~VorbisComment() noexcept {
		vorbis_comment_clear(&vc);
	}

	VorbisComment(const VorbisComment &) = delete;
	VorbisComment &operator=(const VorbisComment &) = delete;

	operator vorbis_comment &() noexcept {
		return vc;
	}

	operator vorbis_comment *() noexcept {
		return &vc;
	}

	void AddTag(const char *tag, const char *contents) noexcept {
		vorbis_comment_add_tag(&vc, tag, contents);
	}
};

#endif
