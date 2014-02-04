/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

struct config_param;
struct Directory;
class SimpleDatabase;
class Error;

/**
 * Check whether the default #SimpleDatabasePlugin is used.  This
 * allows using db_get_root(), db_save(), db_get_mtime() and
 * db_exists().
 */
bool
db_is_simple(void);

gcc_pure
SimpleDatabase &
db_get_simple();

/**
 * Returns true if there is a valid database file on the disk.
 *
 * May only be used if db_is_simple() returns true.
 */
gcc_pure
bool
db_exists();

#endif
