// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_VORBIS_PICTURE_HXX
#define MPD_VORBIS_PICTURE_HXX

#include <string_view>

class TagHandler;

void
ScanVorbisPicture(std::string_view value, TagHandler &handler) noexcept;

#endif
