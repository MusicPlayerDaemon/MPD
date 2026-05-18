// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SQLITE_UTIL_HXX
#define MPD_SQLITE_UTIL_HXX

#include "Error.hxx"

#include <sqlite3.h>

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
 * Call sqlite3_stmt() repepatedly until something other than
 * SQLITE_BUSY is returned.
 */
static inline int
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
static inline bool
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
static inline void
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

} // namespace Sqlite

#endif
