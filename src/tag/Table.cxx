// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Table.hxx"
#include "util/ASCII.hxx"
#include "util/StringCompare.hxx"

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
tag_table_lookup(const struct tag_table *table, std::string_view name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (name == table->name)
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
tag_table_lookup_i(const struct tag_table *table,
		   std::string_view name) noexcept
{
	for (; table->name != nullptr; ++table)
		if (StringIsEqualIgnoreCase(name, table->name))
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
