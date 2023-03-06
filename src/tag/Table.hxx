// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_TABLE_HXX
#define MPD_TAG_TABLE_HXX

#include "Type.hxx"

#include <string_view>

struct tag_table {
	const char *name;

	TagType type;
};

/**
 * Looks up a string in a tag translation table (case sensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
[[gnu::pure]]
TagType
tag_table_lookup(const tag_table *table, const char *name) noexcept;

[[gnu::pure]]
TagType
tag_table_lookup(const tag_table *table, std::string_view name) noexcept;

/**
 * Looks up a string in a tag translation table (case insensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
[[gnu::pure]]
TagType
tag_table_lookup_i(const tag_table *table, const char *name) noexcept;

[[gnu::pure]]
TagType
tag_table_lookup_i(const tag_table *table, std::string_view name) noexcept;

/**
 * Looks up a #TagType in a tag translation table and returns its
 * string representation.  Returns nullptr if the specified type was
 * not found in the table.
 */
[[gnu::pure]]
const char *
tag_table_lookup(const tag_table *table, TagType type) noexcept;

#endif
