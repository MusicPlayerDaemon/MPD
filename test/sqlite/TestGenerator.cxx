// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/sqlite/Generator.hxx"
#include "lib/sqlite/Database.hxx"
#include "lib/sqlite/Error.hxx"
#include "lib/sqlite/Row.hxx"
#include "lib/sqlite/Util.hxx"

#include <gtest/gtest.h>

#include <sqlite3.h>

#include <memory>
#include <string>
#include <vector>

namespace {

struct StatementDeleter {
	void operator()(sqlite3_stmt *stmt) const noexcept {
		sqlite3_finalize(stmt);
	}
};

using UniqueStatement = std::unique_ptr<sqlite3_stmt, StatementDeleter>;

UniqueStatement
PrepareUnique(sqlite3 *db, const char *sql)
{
	return UniqueStatement{Sqlite::Prepare(db, sql)};
}

void
Execute(sqlite3 *db, const char *sql)
{
	Sqlite::ExecuteCommand(PrepareUnique(db, sql).get());
}

/**
 * Create an in-memory database with a table containing three rows;
 * the third one has a NULL name.
 */
Sqlite::Database
CreateTestDatabase()
{
	Sqlite::Database db{":memory:"};
	Execute(db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
	Execute(db, "INSERT INTO t (id, name) VALUES (1, 'one'), (2, 'two'), (3, NULL)");
	return db;
}

/**
 * Collect the given column of all rows; NULL values are
 * represented as "(null)".
 */
std::vector<std::string>
CollectColumn(sqlite3_stmt &stmt, unsigned column)
{
	std::vector<std::string> result;

	for (const Sqlite::Row &row : Sqlite::GenerateRows(stmt)) {
		const char *value = row[column];
		result.emplace_back(value != nullptr ? value : "(null)");
	}

	return result;
}

} // anonymous namespace

TEST(SqliteGenerator, Empty)
{
	const auto db = CreateTestDatabase();
	const auto stmt = PrepareUnique(db, "SELECT name FROM t WHERE id > 100");

	auto rows = Sqlite::GenerateRows(*stmt);
	EXPECT_TRUE(rows.begin() == rows.end());
}

TEST(SqliteGenerator, Rows)
{
	const auto db = CreateTestDatabase();
	const auto stmt = PrepareUnique(db, "SELECT id, name FROM t ORDER BY id");

	unsigned n = 0;
	for (const Sqlite::Row &row : Sqlite::GenerateRows(*stmt)) {
		++n;

		ASSERT_NE(row[0], nullptr);
		EXPECT_EQ(std::string{row[0]}, std::to_string(n));

		switch (n) {
		case 1:
			ASSERT_NE(row[1], nullptr);
			EXPECT_STREQ(row[1], "one");
			break;

		case 2:
			ASSERT_NE(row[1], nullptr);
			EXPECT_STREQ(row[1], "two");
			break;

		case 3:
			/* NULL values are returned as nullptr */
			EXPECT_EQ(row[1], nullptr);
			break;
		}
	}

	EXPECT_EQ(n, 3U);
}

TEST(SqliteGenerator, Bind)
{
	const auto db = CreateTestDatabase();
	const auto stmt = PrepareUnique(db, "SELECT name FROM t WHERE id >= ? ORDER BY id");

	ASSERT_EQ(sqlite3_bind_int(stmt.get(), 1, 2), SQLITE_OK);
	EXPECT_EQ(CollectColumn(*stmt, 0),
		  (std::vector<std::string>{"two", "(null)"}));
}

/**
 * Stop iterating after the first row, then reset the statement and
 * iterate again; the first generator must not have consumed more
 * rows, and the statement must be usable again.
 */
TEST(SqliteGenerator, Break)
{
	const auto db = CreateTestDatabase();
	const auto stmt = PrepareUnique(db, "SELECT id FROM t ORDER BY id");

	{
		auto rows = Sqlite::GenerateRows(*stmt);
		auto i = rows.begin();
		ASSERT_FALSE(i == rows.end());

		const Sqlite::Row &row = *i;
		ASSERT_NE(row[0], nullptr);
		EXPECT_STREQ(row[0], "1");

		/* destroy the generator without advancing it */
	}

	/* the statement is still positioned at the first row */
	EXPECT_EQ(sqlite3_column_int(stmt.get(), 0), 1);

	ASSERT_EQ(sqlite3_reset(stmt.get()), SQLITE_OK);
	EXPECT_EQ(CollectColumn(*stmt, 0),
		  (std::vector<std::string>{"1", "2", "3"}));
}

/**
 * An error in sqlite3_step() after some rows have been returned must
 * be thrown as #SqliteError by the range-based "for" loop.
 */
TEST(SqliteGenerator, Error)
{
	const auto db = CreateTestDatabase();

	/* abs() of the smallest 64 bit integer fails with "integer
	   overflow", but only when evaluated for the third row */
	const auto stmt = PrepareUnique(db,
					"SELECT CASE WHEN id = 3"
					" THEN abs(-9223372036854775807 - 1)"
					" ELSE id END"
					" FROM t ORDER BY id");

	std::vector<std::string> values;

	try {
		for (const Sqlite::Row &row : Sqlite::GenerateRows(*stmt)) {
			ASSERT_NE(row[0], nullptr);
			values.emplace_back(row[0]);
		}

		FAIL() << "No exception thrown";
	} catch (const SqliteError &e) {
		EXPECT_EQ(e.GetCode(), SQLITE_ERROR);
		EXPECT_STREQ(e.what(), "sqlite3_step() failed: integer overflow");
	}

	EXPECT_EQ(values, (std::vector<std::string>{"1", "2"}));
}
