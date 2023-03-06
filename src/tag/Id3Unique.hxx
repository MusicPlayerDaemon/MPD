// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ID3_UNIQUE_HXX
#define MPD_TAG_ID3_UNIQUE_HXX

#include <id3tag.h>

#include <memory>

struct Id3Delete {
	void operator()(struct id3_tag *tag) noexcept {
		id3_tag_delete(tag);
	}
};

using UniqueId3Tag = std::unique_ptr<struct id3_tag, Id3Delete>;

#endif
