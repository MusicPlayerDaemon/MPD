// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
