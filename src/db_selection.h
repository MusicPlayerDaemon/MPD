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

#ifndef MPD_DB_SELECTION_H
#define MPD_DB_SELECTION_H

#include "gcc.h"

#include <assert.h>

struct directory;
struct song;

struct db_selection {
	/**
	 * The base URI of the search (UTF-8).  Must not begin or end
	 * with a slash.  NULL or an empty string searches the whole
	 * database.
	 */
	const char *uri;

	/**
	 * Recursively search all sub directories?
	 */
	bool recursive;
};

gcc_nonnull(1,2)
static inline void
db_selection_init(struct db_selection *selection,
		  const char *uri, bool recursive)
{
	assert(selection != NULL);
	assert(uri != NULL);

	selection->uri = uri;
	selection->recursive = recursive;
}

#endif
