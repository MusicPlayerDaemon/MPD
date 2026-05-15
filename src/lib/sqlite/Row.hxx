// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <sqlite3.h>

namespace Sqlite {

class Row {
	sqlite3_stmt &stmt;

public:
	[[nodiscard]]
	explicit constexpr Row(sqlite3_stmt &_stmt) noexcept
		:stmt(_stmt) {}

	[[gnu::pure]]
	const char *operator[](unsigned column) const noexcept {
		return reinterpret_cast<const char *>(sqlite3_column_text(&stmt, column));
	}
};

} // namespace Sqlite
