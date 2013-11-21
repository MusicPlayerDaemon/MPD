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

#ifndef MPD_DATABASE_SIMPLE_HXX
#define MPD_DATABASE_SIMPLE_HXX

#include "Compiler.h"

#include <sys/time.h>

struct config_param;
struct Directory;
struct db_selection;
struct db_visitor;
class Error;

/**
 * Check whether the default #SimpleDatabasePlugin is used.  This
 * allows using db_get_root(), db_save(), db_get_mtime() and
 * db_exists().
 */
bool
db_is_simple(void);

/**
 * Returns the root directory object.  Returns NULL if there is no
 * configured music directory.
 *
 * May only be used if db_is_simple() returns true.
 */
gcc_pure
Directory *
db_get_root(void);

/**
 * Caller must lock the #db_mutex.
 */
gcc_nonnull(1)
gcc_pure
Directory *
db_get_directory(const char *name);

/**
 * May only be used if db_is_simple() returns true.
 */
bool
db_save(Error &error);

/**
 * Returns true if there is a valid database file on the disk.
 *
 * May only be used if db_is_simple() returns true.
 */
gcc_pure
bool
db_exists();

#endif
