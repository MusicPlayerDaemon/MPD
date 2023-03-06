// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_ID3_SCAN_HXX
#define MPD_TAG_ID3_SCAN_HXX

class InputStream;
class TagHandler;
struct Tag;
struct id3_tag;

/**
 * Throws on I/O error.
 */
bool
tag_id3_scan(InputStream &is, TagHandler &handler);

Tag
tag_id3_import(const struct id3_tag *) noexcept;

/**
 * Import all tags from the provided id3_tag *tag
 *
 */
void
scan_id3_tag(const struct id3_tag *tag, TagHandler &handler) noexcept;

#endif
