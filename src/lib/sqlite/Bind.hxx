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

} // namespace Sqlite
