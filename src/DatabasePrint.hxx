/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "Compiler.h"

class SongFilter;
struct DatabaseSelection;
struct db_visitor;
class Client;
class Error;

bool
db_selection_print(Client &client, const DatabaseSelection &selection,
		   bool full, Error &error);

gcc_nonnull(2)
bool
printAllIn(Client &client, const char *uri_utf8, Error &error);

gcc_nonnull(2)
bool
printInfoForAllIn(Client &client, const char *uri_utf8,
		  Error &error);

gcc_nonnull(2)
bool
searchStatsForSongsIn(Client &client, const char *name,
		      const SongFilter *filter,
		      Error &error);

bool
listAllUniqueTags(Client &client, int type,
		  const SongFilter *filter,
		  Error &error);

#endif
