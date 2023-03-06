// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
