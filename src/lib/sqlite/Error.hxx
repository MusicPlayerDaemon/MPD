// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SQLITE_ERROR_HXX
#define MPD_SQLITE_ERROR_HXX

#include <stdexcept>

struct sqlite3;
struct sqlite3_stmt;

class SqliteError final : public std::runtime_error {
	int code;

public:
	SqliteError(sqlite3 *db, int _code, const char *msg) noexcept;
	SqliteError(sqlite3_stmt *stmt, int _code, const char *msg) noexcept;

	int GetCode() const {
		return code;
	}
};

#endif
