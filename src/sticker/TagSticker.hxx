// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cstdint>

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
MakeSongFilter(const char *sticker_type, const char *sticker_uri);

/**
 * Like MakeSongFilter(const char *sticker_type, const char *sticker_uri)
 * but return an empty filter instead of throwing
 *
 * @param sticker_type
 * @param sticker_uri
 *
 * @return SongFilter
 */
SongFilter
MakeSongFilterNoThrow(const char *sticker_type, const char *sticker_uri) noexcept;

/**
 * Try to make a selection on the database using the tag type and value
 * from a sticker command.
 *
 * @return true if the selection returned at least one match or false otherwise
 *
 * @throws std::runtime_error if failed to make song filter
 */
bool
TagExists(const Database &database, TagType tag_type, const char *tag_value);

/**
 * Try to make a selection on the database using a filter
 * from a sticker command.
 *
 * @return true if the selection returned at least one match or false otherwise
 */
bool
FilterMatches(const Database &database, const SongFilter &filter) noexcept;
