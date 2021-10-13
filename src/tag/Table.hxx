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

#ifndef MPD_TAG_TABLE_HXX
#define MPD_TAG_TABLE_HXX

#include "Type.h"

struct StringView;

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
tag_table_lookup(const tag_table *table, StringView name) noexcept;

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
tag_table_lookup_i(const tag_table *table, StringView name) noexcept;

/**
 * Looks up a #TagType in a tag translation table and returns its
 * string representation.  Returns nullptr if the specified type was
 * not found in the table.
 */
[[gnu::pure]]
const char *
tag_table_lookup(const tag_table *table, TagType type) noexcept;

#endif
