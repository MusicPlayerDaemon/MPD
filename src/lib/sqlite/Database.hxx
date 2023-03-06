// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SQLITE_DATABASE_HXX
#define MPD_SQLITE_DATABASE_HXX

#include <sqlite3.h>

#include <utility>

namespace Sqlite {

class Database {
	sqlite3 *db = nullptr;

public:
	Database() = default;

	explicit Database(const char *path);

	~Database() noexcept {
		if (db != nullptr)
			sqlite3_close(db);
	}

	Database(Database &&src) noexcept
		:db(std::exchange(src.db, nullptr)) {}

	Database &operator=(Database &&src) noexcept {
		using std::swap;
		swap(db, src.db);
		return *this;
	}

	operator sqlite3 *() const noexcept {
		return db;
	}
};

} // namespace Sqlite

#endif
