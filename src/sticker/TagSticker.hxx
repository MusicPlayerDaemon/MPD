/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#ifndef TAG_STICKER_HXX
#define TAG_STICKER_HXX

#include <string>

enum TagType : uint8_t;
class Database;
class StickerDatabase;
class SongFilter;

/**
 * Parse a filter_string.
 *
 * @param filter_string a valid filter expression
 *
 * @return SongFilter
 *
 * @throws std::runtime_error if failed to parse filter string
 */
SongFilter
MakeSongFilter(const char *filter_string);

/**
 * Make a song filter from tag and value e.g. album name
 *
 * @return SongFilter
 *
 * @throws std::runtime_error if failed to make song filter or tag type not allowd for sticker
 */
SongFilter
MakeSongFilter(TagType tag_type, const char *tag_value);

/**
 * Make a song filter by sticker type and uri
 *
 * @param sticker_type	either one of the allowed tag names or "filter"
 * @param sticker_uri	if the type is a tag name then this is the value,
 * 			if the type if "filter" then this is a filter expression
 *
 * @return SongFilter
 *
 * @throws std::runtime_error if failed to make song filter or tag type not allowd for sticker
 */
SongFilter
MakeSongFilter(const std::string &sticker_type, const std::string &sticker_uri);

/**
 * Like MakeSongFilter(const std::string &sticker_type, const std::string &sticker_uri)
 * but return an empty filter instead of throwing
 *
 * @param sticker_type
 * @param sticker_uri
 *
 * @return SongFilter
 */
SongFilter
MakeSongFilterNoThrow(const std::string &sticker_type, const std::string &sticker_uri) noexcept;

/**
 * Try to make a selection on the database using the tag type and value
 * from a sticker command.
 *
 * @return true if the selection returned at least one match or false otherwise
 *
 * @throws std::runtime_error if failed to make song filter
 */
bool
TagExists(const Database& database, TagType tag_type, const char* tag_value);

/**
 * Try to make a selection on the database using a filter
 * from a sticker command.
 *
 * @return true if the selection returned at least one match or false otherwise
 */
bool
FilterMatches(const Database& database, const SongFilter& filter) noexcept;


#endif //TAG_STICKER_HXX
