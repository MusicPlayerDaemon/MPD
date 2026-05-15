// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Generator.hxx"
#include "Row.hxx"
#include "Util.hxx"

#include <sqlite3.h>

namespace Sqlite {

Co::Generator<Row>
GenerateRows(sqlite3_stmt &stmt)
{
	while (true) {
		int result = ExecuteBusy(&stmt);
		switch (result) {
		case SQLITE_ROW:
			co_yield Row{stmt};
			break;

		case SQLITE_DONE:
			co_return;

		default:
			throw SqliteError{&stmt, result, "sqlite3_step() failed"};
		}
	}
}

} // namespace Sqlite
