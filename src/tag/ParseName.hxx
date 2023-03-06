// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_PARSE_NAME_HXX
#define MPD_TAG_PARSE_NAME_HXX

#include <cstdint>

#include <string_view>

enum TagType : uint8_t;

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 */
[[gnu::pure]]
TagType
tag_name_parse(const char *name) noexcept;

[[gnu::pure]]
TagType
tag_name_parse(std::string_view name) noexcept;

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 *
 * Case does not matter.
 */
[[gnu::pure]]
TagType
tag_name_parse_i(const char *name) noexcept;

[[gnu::pure]]
TagType
tag_name_parse_i(std::string_view name) noexcept;

#endif
