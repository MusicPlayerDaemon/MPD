/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_TAG_TABLE_H
#define MPD_TAG_TABLE_H

#include "tag.h"

#include <glib.h>

struct tag_table {
	const char *name;

	enum tag_type type;
};

/**
 * Looks up a string in a tag translation table (case sensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
G_GNUC_PURE
static inline enum tag_type
tag_table_lookup(const struct tag_table *table, const char *name)
{
	for (; table->name != NULL; ++table)
		if (strcmp(name, table->name) == 0)
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

/**
 * Looks up a string in a tag translation table (case insensitive).
 * Returns TAG_NUM_OF_ITEM_TYPES if the specified name was not found
 * in the table.
 */
G_GNUC_PURE
static inline enum tag_type
tag_table_lookup_i(const struct tag_table *table, const char *name)
{
	for (; table->name != NULL; ++table)
		if (g_ascii_strcasecmp(name, table->name) == 0)
			return table->type;

	return TAG_NUM_OF_ITEM_TYPES;
}

#endif
