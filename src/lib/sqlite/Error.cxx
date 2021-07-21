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

#include "Error.hxx"

#include <sqlite3.h>

#include <string>

static std::string
MakeSqliteErrorMessage(sqlite3 *db, const char *msg) noexcept
{
	return std::string(msg) + ": " + sqlite3_errmsg(db);
}

SqliteError::SqliteError(sqlite3 *db, int _code, const char *msg) noexcept
	:std::runtime_error(MakeSqliteErrorMessage(db, msg)), code(_code) {}

SqliteError::SqliteError(sqlite3_stmt *stmt, int _code,
			 const char *msg) noexcept
	:SqliteError(sqlite3_db_handle(stmt), _code, msg) {}
