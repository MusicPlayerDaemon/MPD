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
