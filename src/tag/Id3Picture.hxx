// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ID3_PICTURE_HXX
#define MPD_TAG_ID3_PICTURE_HXX

#include <span>

class TagHandler;

/**
 * Scan an "APIC" value and call TagHandler::OnPicture().
 */
void
ScanId3Apic(std::span<const std::byte> buffer, TagHandler &handler) noexcept;

#endif
