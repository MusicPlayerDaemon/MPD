// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Database.hxx"
#include "Error.hxx"
#include "util/StringFormat.hxx"

namespace Sqlite {

Database::Database(const char *path)
{
	int result = sqlite3_open(path, &db);
	if (result != SQLITE_OK)
		throw SqliteError(db, result,
				  StringFormat<1024>("Failed to open sqlite database '%s'",
						     path));
}

} // namespace Sqlite
