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

#include "Table.hxx"
#include "util/ASCII.hxx"
#include "util/StringView.hxx"

#include <string.h>

/**
 * Looks up a string in a tag translation table (case sensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
TagType
tag_table_lookup(const struct tag_table *table, const char *name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (strcmp(name, table->name) == 0)
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

TagType
tag_table_lookup(const struct tag_table *table, StringView name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (name.Equals(table->name))
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

/**
 * Looks up a string in a tag translation table (case insensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
TagType
tag_table_lookup_i(const struct tag_table *table, const char *name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (StringEqualsCaseASCII(name, table->name))
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

TagType
tag_table_lookup_i(const struct tag_table *table, StringView name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (name.EqualsIgnoreCase(table->name))
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

const char *
tag_table_lookup(const tag_table *table, TagType type) noexcept
{
	for (; table->name != nullptr; ++table)
		if (table->type == type)
			return table->name;

	return nullptr;
}
