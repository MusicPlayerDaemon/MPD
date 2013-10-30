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

#ifndef MPD_DATABASE_GLUE_HXX
#define MPD_DATABASE_GLUE_HXX

#include "Compiler.h"

struct config_param;
class Database;
class Error;

/**
 * Initialize the database library.
 *
 * @param param the database configuration block
 */
bool
DatabaseGlobalInit(const config_param &param, Error &error);

void
DatabaseGlobalDeinit(void);

bool
DatabaseGlobalOpen(Error &error);

/**
 * Returns the global #Database instance.  May return nullptr if this MPD
 * configuration has no database (no music_directory was configured).
 */
gcc_pure
const Database *
GetDatabase();

/**
 * Returns the global #Database instance.  May return nullptr if this MPD
 * configuration has no database (no music_directory was configured).
 */
gcc_pure
const Database *
GetDatabase(Error &error);

#endif
