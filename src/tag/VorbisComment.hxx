// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_VORBIS_COMMENT_HXX
#define MPD_TAG_VORBIS_COMMENT_HXX

#include <string_view>

/**
 * Checks if the specified name matches the entry's name, and if yes,
 * returns the comment value.
 */
[[gnu::pure]]
std::string_view
GetVorbisCommentValue(std::string_view entry, std::string_view name) noexcept;

#endif
