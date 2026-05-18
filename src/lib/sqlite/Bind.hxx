// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Error.hxx"

#include <sqlite3.h>

#include <cassert>

namespace Sqlite {

/**
 * Throws #SqliteError on error.
 */
static inline void
Bind(sqlite3_stmt *stmt, unsigned i, const char *value)
{
	int result = sqlite3_bind_text(stmt, i, value, -1, nullptr);
	if (result != SQLITE_OK)
		throw SqliteError(stmt, result, "sqlite3_bind_text() failed");
}

/**
 * Throws #SqliteError on error.
 */
template<typename... Args>
static void
BindAll(sqlite3_stmt *stmt, Args&&... args)
{
	assert(int(sizeof...(args)) == sqlite3_bind_parameter_count(stmt));

	unsigned i = 1;
	(Bind(stmt, i++, std::forward<Args>(args)), ...);
}

} // namespace Sqlite
