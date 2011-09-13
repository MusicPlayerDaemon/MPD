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

#ifndef MPD_DB_PRINT_H
#define MPD_DB_PRINT_H

#include "gcc.h"

#include <glib.h>
#include <stdbool.h>

struct client;
struct locate_item_list;
struct db_selection;
struct db_visitor;

gcc_nonnull(1,2)
bool
db_selection_print(struct client *client, const struct db_selection *selection,
		   bool full, GError **error_r);

gcc_nonnull(1,2)
bool
printAllIn(struct client *client, const char *uri_utf8, GError **error_r);

gcc_nonnull(1,2)
bool
printInfoForAllIn(struct client *client, const char *uri_utf8,
		  GError **error_r);

gcc_nonnull(1,2,3)
bool
searchForSongsIn(struct client *client, const char *name,
		 const struct locate_item_list *criteria,
		 GError **error_r);

gcc_nonnull(1,2,3)
bool
findSongsIn(struct client *client, const char *name,
	    const struct locate_item_list *criteria,
	    GError **error_r);

gcc_nonnull(1,2,3)
bool
searchStatsForSongsIn(struct client *client, const char *name,
		      const struct locate_item_list *criteria,
		      GError **error_r);

gcc_nonnull(1,3)
bool
listAllUniqueTags(struct client *client, int type,
		  const struct locate_item_list *criteria,
		  GError **error_r);

#endif
