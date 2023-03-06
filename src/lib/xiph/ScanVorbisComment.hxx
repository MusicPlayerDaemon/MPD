// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SCAN_VORBIS_COMMENT_HXX
#define MPD_SCAN_VORBIS_COMMENT_HXX

#include <string_view>

class TagHandler;

void
ScanVorbisComment(std::string_view comment, TagHandler &handler) noexcept;

#endif
