/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "util/Error.hxx"

#include <sqlite3.h>

#include <assert.h>

static void
SetError(Error &error, sqlite3 *db, int code, const char *msg)
{
	error.Format(sqlite_domain, code, "%s: %s",
		     msg, sqlite3_errmsg(db));
}

static void
SetError(Error &error, sqlite3_stmt *stmt, int code, const char *msg)
{
	SetError(error, sqlite3_db_handle(stmt), code, msg);
}

static bool
Bind(sqlite3_stmt *stmt, unsigned i, const char *value, Error &error)
{
	int result = sqlite3_bind_text(stmt, i, value, -1, nullptr);
	if (result != SQLITE_OK) {
		SetError(error, stmt, result, "sqlite3_bind_text() failed");
		return false;
	}

	return true;
}

template<typename... Args>
static bool
BindAll2(gcc_unused Error &error, gcc_unused sqlite3_stmt *stmt,
	 gcc_unused unsigned i)
{
	assert(int(i - 1) == sqlite3_bind_parameter_count(stmt));

	return true;
}

template<typename... Args>
static bool
BindAll2(Error &error, sqlite3_stmt *stmt, unsigned i,
	 const char *value, Args&&... args)
{
	return Bind(stmt, i, value, error) &&
		BindAll2(error, stmt, i + 1, std::forward<Args>(args)...);
}

template<typename... Args>
static bool
BindAll(Error &error, sqlite3_stmt *stmt, Args&&... args)
{
	assert(int(sizeof...(args)) == sqlite3_bind_parameter_count(stmt));

	return BindAll2(error, stmt, 1, std::forward<Args>(args)...);
}

/**
 * Wrapper for BindAll() that returns the specified sqlite3_stmt* on
 * success and nullptr on error.
 */
template<typename... Args>
static sqlite3_stmt *
BindAllOrNull(Error &error, sqlite3_stmt *stmt, Args&&... args)
{
	return BindAll(error, stmt, std::forward<Args>(args)...)
		? stmt
		: nullptr;
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
ExecuteRow(sqlite3_stmt *stmt, Error &error)
{
	int result = ExecuteBusy(stmt);
	if (result == SQLITE_ROW)
		return true;

	if (result != SQLITE_DONE)
		SetError(error, stmt, result, "sqlite3_step() failed");

	return false;
}

/**
 * Wrapper for ExecuteBusy() that interprets everything other than
 * SQLITE_DONE as error.
 */
static bool
ExecuteCommand(sqlite3_stmt *stmt, Error &error)
{
	int result = ExecuteBusy(stmt);
	if (result != SQLITE_DONE) {
		SetError(error, stmt, result, "sqlite3_step() failed");
		return false;
	}

	return true;
}

/**
 * Wrapper for ExecuteCommand() that returns the number of rows
 * modified via sqlite3_changes().  Returns -1 on error.
 */
static inline int
ExecuteChanges(sqlite3_stmt *stmt, Error &error)
{
	if (!ExecuteCommand(stmt, error))
		return -1;

	return sqlite3_changes(sqlite3_db_handle(stmt));
}

/**
 * Wrapper for ExecuteChanges() that returns true if at least one row
 * was modified.  Returns false if nothing was modified or if an error
 * occurred.
 */
static inline bool
ExecuteModified(sqlite3_stmt *stmt, Error &error)
{
	return ExecuteChanges(stmt, error) > 0;
}

template<typename F>
static inline bool
ExecuteForEach(sqlite3_stmt *stmt, Error &error, F &&f)
{
	while (true) {
		int result = ExecuteBusy(stmt);
		switch (result) {
		case SQLITE_ROW:
			f();
			break;

		case SQLITE_DONE:
			return true;

		default:
			SetError(error, stmt, result, "sqlite3_step() failed");
			return false;
		}
	}
}

#endif
