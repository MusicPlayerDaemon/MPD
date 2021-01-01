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
