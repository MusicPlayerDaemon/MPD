// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Database.hxx"
#include "Sticker.hxx"
#include "lib/sqlite/Util.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "Idle.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"

#include <fmt/format.h>
#include <cassert>
#include <iterator>
#include <array>
#include <stdexcept>

using namespace Sqlite;

enum sticker_sql_find {
	STICKER_SQL_FIND,
	STICKER_SQL_FIND_VALUE,
	STICKER_SQL_FIND_LT,
	STICKER_SQL_FIND_GT,

	STICKER_SQL_FIND_EQ_INT,
	STICKER_SQL_FIND_LT_INT,
	STICKER_SQL_FIND_GT_INT,

	STICKER_SQL_FIND_COUNT
};

enum sticker_sql {
	STICKER_SQL_GET,
	STICKER_SQL_LIST,
	STICKER_SQL_UPDATE,
	STICKER_SQL_INSERT,
	STICKER_SQL_DELETE,
	STICKER_SQL_DELETE_VALUE,
	STICKER_SQL_DISTINCT_TYPE_URI,
	STICKER_SQL_TRANSACTION_BEGIN,
	STICKER_SQL_TRANSACTION_COMMIT,
	STICKER_SQL_TRANSACTION_ROLLBACK,
	STICKER_SQL_NAMES,

	STICKER_SQL_COUNT
};

static constexpr auto sticker_sql_find = std::array {
	//[STICKER_SQL_FIND] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=?",

	//[STICKER_SQL_FIND_VALUE] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value=?",

	//[STICKER_SQL_FIND_LT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value<?",

	//[STICKER_SQL_FIND_GT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value>?",

	//[STICKER_SQL_FIND_EQ_INT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND CAST(value AS INT)=?",

	//[STICKER_SQL_FIND_LT_INT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND CAST(value AS INT)<?",

	//[STICKER_SQL_FIND_GT_INT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND CAST(value AS INT)>?",
};

static constexpr auto sticker_sql = std::array {
	//[STICKER_SQL_GET] =
	"SELECT value FROM sticker WHERE type=? AND uri=? AND name=?",
	//[STICKER_SQL_LIST] =
	"SELECT name,value FROM sticker WHERE type=? AND uri=?",
	//[STICKER_SQL_UPDATE] =
	"UPDATE sticker SET value=? WHERE type=? AND uri=? AND name=?",
	//[STICKER_SQL_INSERT] =
	"INSERT INTO sticker(type,uri,name,value) VALUES(?, ?, ?, ?)",
	//[STICKER_SQL_DELETE] =
	"DELETE FROM sticker WHERE type=? AND uri=?",
	//[STICKER_SQL_DELETE_VALUE] =
	"DELETE FROM sticker WHERE type=? AND uri=? AND name=?",

	//[STICKER_SQL_DISTINCT_TYPE_URI] =
	"SELECT DISTINCT type,uri FROM sticker",

	//[STICKER_SQL_TRANSACTION_BEGIN]
	"BEGIN",

	//[STICKER_SQL_TRANSACTION_COMMIT]
	"COMMIT",

	//[STICKER_SQL_TRANSACTION_ROLLBACK]
	"ROLLBACK",

	//[STICKER_SQL_NAMES]
	"SELECT DISTINCT name FROM sticker order by name",
};

static constexpr const char sticker_sql_create[] =
	"CREATE TABLE IF NOT EXISTS sticker("
	"  type VARCHAR NOT NULL, "
	"  uri VARCHAR NOT NULL, "
	"  name VARCHAR NOT NULL, "
	"  value VARCHAR NOT NULL"
	");"
	"CREATE UNIQUE INDEX IF NOT EXISTS"
	" sticker_value ON sticker(type, uri, name);"
	"";

StickerDatabase::StickerDatabase(const char *_path)
	:path(_path),
	 db(path.c_str())
{
	int ret;

	/* create the table and index */

	ret = sqlite3_exec(db, sticker_sql_create,
			   nullptr, nullptr, nullptr);
	if (ret != SQLITE_OK)
		throw SqliteError(db, ret,
				  "Failed to create sticker table");

	/* prepare the statements we're going to use */

	for (size_t i = 0; i < sticker_sql.size(); ++i) {
		assert(sticker_sql[i] != nullptr);

		stmt[i] = Prepare(db, sticker_sql[i]);
	}
}

StickerDatabase::StickerDatabase(Path _path)
	:StickerDatabase(NarrowPath{_path}) {}

StickerDatabase::~StickerDatabase() noexcept
{
	if (db == nullptr)
		return;

	for (const auto &sticker : stmt) {
		assert(sticker != nullptr);

		sqlite3_finalize(sticker);
	}
}

std::string
StickerDatabase::LoadValue(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_GET];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);

	if (StringIsEmpty(name))
		return {};

	BindAll(s, type, uri, name);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	std::string value;
	if (ExecuteRow(s))
		value = (const char*)sqlite3_column_text(s, 0);

	return value;
}

void
StickerDatabase::ListValues(std::map<std::string, std::string, std::less<>> &table,
			    const char *type, const char *uri)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_LIST];

	assert(type != nullptr);
	assert(uri != nullptr);

	BindAll(s, type, uri);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	ExecuteForEach(s, [s, &table](){
		const char *name = (const char *)sqlite3_column_text(s, 0);
		const char *value = (const char *)sqlite3_column_text(s, 1);
		table.emplace(name, value);
	});
}

bool
StickerDatabase::UpdateValue(const char *type, const char *uri,
			     const char *name, const char *value)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_UPDATE];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	BindAll(s, value, type, uri, name);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	bool modified = ExecuteModified(s);

	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

void
StickerDatabase::InsertValue(const char *type, const char *uri,
			     const char *name, const char *value)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_INSERT];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	BindAll(s, type, uri, name, value);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	ExecuteCommand(s);
	idle_add(IDLE_STICKER);
}

void
StickerDatabase::StoreValue(const char *type, const char *uri,
			    const char *name, const char *value)
{
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(value != nullptr);

	if (StringIsEmpty(name))
		return;

	if (!UpdateValue(type, uri, name, value))
		InsertValue(type, uri, name, value);
}

bool
StickerDatabase::Delete(const char *type, const char *uri)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_DELETE];

	assert(type != nullptr);
	assert(uri != nullptr);

	BindAll(s, type, uri);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	bool modified = ExecuteModified(s);
	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

bool
StickerDatabase::DeleteValue(const char *type, const char *uri,
			     const char *name)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_DELETE_VALUE];

	assert(type != nullptr);
	assert(uri != nullptr);

	BindAll(s, type, uri, name);

	AtScopeExit(s) {
		sqlite3_reset(s);
		sqlite3_clear_bindings(s);
	};

	bool modified = ExecuteModified(s);
	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

Sticker
StickerDatabase::Load(const char *type, const char *uri)
{
	Sticker s;

	ListValues(s.table, type, uri);

	return s;
}

sqlite3_stmt *
StickerDatabase::BindFind(const char *type, const char *base_uri,
			  const char *name,
			  StickerOperator op, const char *value,
			  const char *sort, bool descending, RangeArg window)
{
	assert(type != nullptr);
	assert(name != nullptr);

	if (base_uri == nullptr)
		base_uri = "";

	auto order_by = StringIsEmpty(sort)
		? std::string()
		: StringIsEqual(sort, "value_int")
			? fmt::format("ORDER BY CAST(value AS INT) {}", descending ? "desc" : "asc")
			: fmt::format("ORDER BY {} {}", sort, descending ? "desc" : "asc");

	auto offset = window.IsAll()
		? std::string()
		: window.IsOpenEnded()
			? fmt::format("LIMIT -1 OFFSET {}", window.start)
			: fmt::format("LIMIT {} OFFSET {}", window.Count(), window.start);

	std::string sql_str;
	sqlite3_stmt *sql;

	switch (op) {
	case StickerOperator::EXISTS:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name);
		return sql;

	case StickerOperator::EQUALS:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_VALUE], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;

	case StickerOperator::LESS_THAN:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_LT], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;

	case StickerOperator::GREATER_THAN:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_GT], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;

	case StickerOperator::EQUALS_INT:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_EQ_INT], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;

	case StickerOperator::LESS_THAN_INT:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_LT_INT], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;

	case StickerOperator::GREATER_THAN_INT:
		sql_str = fmt::format("{} {} {}",
			sticker_sql_find[STICKER_SQL_FIND_GT_INT], order_by, offset);
		sql = Prepare(db, sql_str.c_str());
		BindAll(sql, type, base_uri, name, value);
		return sql;
	}

	assert(false);
	gcc_unreachable();
}

void
StickerDatabase::Find(const char *type, const char *base_uri, const char *name,
		      StickerOperator op, const char *value,
			  const char *sort, bool descending, RangeArg window,
		      void (*func)(const char *uri, const char *value,
				   void *user_data),
		      void *user_data)
{
	assert(func != nullptr);

	sqlite3_stmt *const s = BindFind(type, base_uri, name, op, value, sort, descending, window);
	assert(s != nullptr);

	AtScopeExit(s) {
		sqlite3_finalize(s);
	};

	ExecuteForEach(s, [s, func, user_data](){
			func((const char*)sqlite3_column_text(s, 0),
			     (const char*)sqlite3_column_text(s, 1),
			     user_data);
		});
}

std::list<StickerDatabase::StickerTypeUriPair>
StickerDatabase::GetUniqueStickers()
{
	auto result = std::list<StickerTypeUriPair>{};
	sqlite3_stmt *const s = stmt[STICKER_SQL_DISTINCT_TYPE_URI];
	assert(s != nullptr);
	AtScopeExit(s) {
		sqlite3_reset(s);
	};
	ExecuteForEach(s, [&s, &result]() {
		result.emplace_back((const char*)sqlite3_column_text(s, 0),
				    (const char*)sqlite3_column_text(s, 1));
	});
	return result;
}

void
StickerDatabase::Names(void (*func)(const char *value, void *user_data), void *user_data)
{
	assert(func != nullptr);

	sqlite3_stmt *const s = stmt[STICKER_SQL_NAMES];
	assert(s != nullptr);

	AtScopeExit(s) {
		sqlite3_reset(s);
	};

	ExecuteForEach(s, [s, func, user_data](){
			func((const char*)sqlite3_column_text(s, 0), user_data);
		});
}

void
StickerDatabase::BatchDeleteNoIdle(const std::list<StickerTypeUriPair> &stickers)
{
	sqlite3_stmt *const s = stmt[STICKER_SQL_DELETE];

	sqlite3_stmt *const begin = stmt[STICKER_SQL_TRANSACTION_BEGIN];
	sqlite3_stmt *const rollback = stmt[STICKER_SQL_TRANSACTION_ROLLBACK];
	sqlite3_stmt *const commit = stmt[STICKER_SQL_TRANSACTION_COMMIT];

	try {
		ExecuteBusy(begin);

		for (auto &sticker: stickers) {
			AtScopeExit(s) {
				sqlite3_reset(s);
				sqlite3_clear_bindings(s);
			};

			BindAll(s, sticker.first.c_str(), sticker.second.c_str());

			ExecuteCommand(s);
		}

		ExecuteBusy(commit);
	} catch (...) {
		// "If the transaction has already been rolled back automatically by the error response,
		// then the ROLLBACK command will fail with an error, but no harm is caused by this."
		ExecuteBusy(rollback);
		std::throw_with_nested(std::runtime_error{"failed to batch-delete stickers"});
	}
}
