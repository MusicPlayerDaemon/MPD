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

#ifndef MPD_SQLITE_UTIL_HXX
#define MPD_SQLITE_UTIL_HXX

#include "Domain.hxx"
#include "Log.hxx"

#include <sqlite3.h>

#include <assert.h>

static void
LogError(sqlite3 *db, const char *msg)
{
	FormatError(sqlite_domain, "%s: %s", msg, sqlite3_errmsg(db));
}

static void
LogError(sqlite3_stmt *stmt, const char *msg)
{
	LogError(sqlite3_db_handle(stmt), msg);
}

static bool
Bind(sqlite3_stmt *stmt, unsigned i, const char *value)
{
	int result = sqlite3_bind_text(stmt, i, value, -1, nullptr);
	if (result != SQLITE_OK) {
		LogError(stmt, "sqlite3_bind_text() failed");
		return false;
	}

	return true;
}

template<typename... Args>
static bool
BindAll2(gcc_unused sqlite3_stmt *stmt, gcc_unused unsigned i)
{
	assert(int(i - 1) == sqlite3_bind_parameter_count(stmt));

	return true;
}

template<typename... Args>
static bool
BindAll2(sqlite3_stmt *stmt, unsigned i, const char *value, Args&&... args)
{
	return Bind(stmt, i, value) &&
		BindAll2(stmt, i + 1, std::forward<Args>(args)...);
}

template<typename... Args>
static bool
BindAll(sqlite3_stmt *stmt, Args&&... args)
{
	assert(int(sizeof...(args)) == sqlite3_bind_parameter_count(stmt));

	return BindAll2(stmt, 1, std::forward<Args>(args)...);
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
 */
static bool
ExecuteRow(sqlite3_stmt *stmt)
{
	int result = ExecuteBusy(stmt);
	if (result == SQLITE_ROW)
		return true;

	if (result != SQLITE_DONE)
		LogError(sqlite_domain, "sqlite3_step() failed");

	return false;
}

/**
 * Wrapper for ExecuteBusy() that interprets everything other than
 * SQLITE_DONE as error.
 */
static bool
ExecuteCommand(sqlite3_stmt *stmt)
{
	int result = ExecuteBusy(stmt);
	if (result != SQLITE_DONE) {
		LogError(stmt, "sqlite3_step() failed");
		return false;
	}

	return true;
}

/**
 * Wrapper for ExecuteCommand() that returns the number of rows
 * modified via sqlite3_changes().  Returns -1 on error.
 */
static inline int
ExecuteChanges(sqlite3_stmt *stmt)
{
	if (!ExecuteCommand(stmt))
		return -1;

	return sqlite3_changes(sqlite3_db_handle(stmt));
}

template<typename F>
static inline bool
ExecuteForEach(sqlite3_stmt *stmt, F &&f)
{
	while (true) {
		switch (ExecuteBusy(stmt)) {
		case SQLITE_ROW:
			f();
			break;

		case SQLITE_DONE:
			return true;

		default:
			LogError(sqlite_domain, "sqlite3_step() failed");
			return false;
		}
	}
}

#endif
