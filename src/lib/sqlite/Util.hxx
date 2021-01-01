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

#ifndef MPD_SQLITE_UTIL_HXX
#define MPD_SQLITE_UTIL_HXX

#include "util/Compiler.h"
#include "Error.hxx"

#include <sqlite3.h>

#include <cassert>

namespace Sqlite {

static inline sqlite3_stmt *
Prepare(sqlite3 *db, const char *sql)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (ret != SQLITE_OK)
		throw SqliteError(db, ret,
				  "sqlite3_prepare_v2() failed");

	return stmt;
}

/**
 * Throws #SqliteError on error.
 */
static void
Bind(sqlite3_stmt *stmt, unsigned i, const char *value)
{
	int result = sqlite3_bind_text(stmt, i, value, -1, nullptr);
	if (result != SQLITE_OK)
		throw SqliteError(stmt, result, "sqlite3_bind_text() failed");
}

template<typename... Args>
static void
BindAll2([[maybe_unused]] sqlite3_stmt *stmt, [[maybe_unused]] unsigned i)
{
	assert(int(i - 1) == sqlite3_bind_parameter_count(stmt));
}

template<typename... Args>
static void
BindAll2(sqlite3_stmt *stmt, unsigned i,
	 const char *value, Args&&... args)
{
	Bind(stmt, i, value);
	BindAll2(stmt, i + 1, std::forward<Args>(args)...);
}

/**
 * Throws #SqliteError on error.
 */
template<typename... Args>
static void
BindAll(sqlite3_stmt *stmt, Args&&... args)
{
	assert(int(sizeof...(args)) == sqlite3_bind_parameter_count(stmt));

	BindAll2(stmt, 1, std::forward<Args>(args)...);
}

/**
 * Call sqlite3_stmt() repepatedly until something other than
 * SQLITE_BUSY is returned.
 */
static int
ExecuteBusy(sqlite3_stmt *stmt)
{
	int result;
	do {
		result = sqlite3_step(stmt);
	} while (result == SQLITE_BUSY);

	return result;
}

/**
 * Wrapper for ExecuteBusy() that returns true on SQLITE_ROW.
 *
 * Throws #SqliteError on error.
 */
static bool
ExecuteRow(sqlite3_stmt *stmt)
{
	int result = ExecuteBusy(stmt);
	if (result == SQLITE_ROW)
		return true;

	if (result != SQLITE_DONE)
		throw SqliteError(stmt, result, "sqlite3_step() failed");

	return false;
}

/**
 * Wrapper for ExecuteBusy() that interprets everything other than
 * SQLITE_DONE as error.
 *
 * Throws #SqliteError on error.
 */
static void
ExecuteCommand(sqlite3_stmt *stmt)
{
	int result = ExecuteBusy(stmt);
	if (result != SQLITE_DONE)
		throw SqliteError(stmt, result, "sqlite3_step() failed");
}

/**
 * Wrapper for ExecuteCommand() that returns the number of rows
 * modified via sqlite3_changes().
 *
 * Throws #SqliteError on error.
 */
static inline unsigned
ExecuteChanges(sqlite3_stmt *stmt)
{
	ExecuteCommand(stmt);

	return sqlite3_changes(sqlite3_db_handle(stmt));
}

/**
 * Wrapper for ExecuteChanges() that returns true if at least one row
 * was modified.  Returns false if nothing was modified.
 *
 * Throws #SqliteError on error.
 */
static inline bool
ExecuteModified(sqlite3_stmt *stmt)
{
	return ExecuteChanges(stmt) > 0;
}

template<typename F>
static inline void
ExecuteForEach(sqlite3_stmt *stmt, F &&f)
{
	while (true) {
		int result = ExecuteBusy(stmt);
		switch (result) {
		case SQLITE_ROW:
			f();
			break;

		case SQLITE_DONE:
			return;

		default:
			throw SqliteError(stmt, result, "sqlite3_step() failed");
		}
	}
}

} // namespace Sqlite

#endif
