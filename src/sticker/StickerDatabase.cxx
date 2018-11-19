/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "StickerDatabase.hxx"
#include "lib/sqlite/Util.hxx"
#include "fs/Path.hxx"
#include "Idle.hxx"
#include "util/Macros.hxx"
#include "util/StringCompare.hxx"
#include "util/ScopeExit.hxx"

#include <string>
#include <map>

#include <assert.h>

struct Sticker {
	std::map<std::string, std::string> table;
};

enum sticker_sql {
	STICKER_SQL_GET,
	STICKER_SQL_LIST,
	STICKER_SQL_UPDATE,
	STICKER_SQL_INSERT,
	STICKER_SQL_DELETE,
	STICKER_SQL_DELETE_VALUE,
	STICKER_SQL_FIND,
	STICKER_SQL_FIND_VALUE,
	STICKER_SQL_FIND_LT,
	STICKER_SQL_FIND_GT,
};

static const char *const sticker_sql[] = {
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
	//[STICKER_SQL_FIND] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=?",

	//[STICKER_SQL_FIND_VALUE] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value=?",

	//[STICKER_SQL_FIND_LT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value<?",

	//[STICKER_SQL_FIND_GT] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=? AND value>?",
};

static const char sticker_sql_create[] =
	"CREATE TABLE IF NOT EXISTS sticker("
	"  type VARCHAR NOT NULL, "
	"  uri VARCHAR NOT NULL, "
	"  name VARCHAR NOT NULL, "
	"  value VARCHAR NOT NULL"
	");"
	"CREATE UNIQUE INDEX IF NOT EXISTS"
	" sticker_value ON sticker(type, uri, name);"
	"";

static sqlite3 *sticker_db;
static sqlite3_stmt *sticker_stmt[ARRAY_SIZE(sticker_sql)];

static sqlite3_stmt *
sticker_prepare(const char *sql)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(sticker_db, sql, -1, &stmt, nullptr);
	if (ret != SQLITE_OK)
		throw SqliteError(sticker_db, ret,
				  "sqlite3_prepare_v2() failed");

	return stmt;
}

void
sticker_global_init(Path path)
{
	assert(!path.IsNull());

	int ret;

	/* open/create the sqlite database */

	ret = sqlite3_open(path.c_str(), &sticker_db);
	if (ret != SQLITE_OK) {
		const std::string utf8 = path.ToUTF8();
		throw SqliteError(sticker_db, ret,
				  ("Failed to open sqlite database '" +
				   utf8 + "'").c_str());
	}

	/* create the table and index */

	ret = sqlite3_exec(sticker_db, sticker_sql_create,
			   nullptr, nullptr, nullptr);
	if (ret != SQLITE_OK)
		throw SqliteError(sticker_db, ret,
				  "Failed to create sticker table");

	/* prepare the statements we're going to use */

	for (unsigned i = 0; i < ARRAY_SIZE(sticker_sql); ++i) {
		assert(sticker_sql[i] != nullptr);

		sticker_stmt[i] = sticker_prepare(sticker_sql[i]);
	}
}

void
sticker_global_finish()
{
	if (sticker_db == nullptr)
		/* not configured */
		return;

	for (unsigned i = 0; i < ARRAY_SIZE(sticker_stmt); ++i) {
		assert(sticker_stmt[i] != nullptr);

		sqlite3_finalize(sticker_stmt[i]);
	}

	sqlite3_close(sticker_db);
}

bool
sticker_enabled() noexcept
{
	return sticker_db != nullptr;
}

std::string
sticker_load_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_GET];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);

	if (StringIsEmpty(name))
		return std::string();

	BindAll(stmt, type, uri, name);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	std::string value;
	if (ExecuteRow(stmt))
		value = (const char*)sqlite3_column_text(stmt, 0);

	return value;
}

static void
sticker_list_values(std::map<std::string, std::string> &table,
		    const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_LIST];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(sticker_enabled());

	BindAll(stmt, type, uri);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	ExecuteForEach(stmt, [stmt, &table](){
			const char *name = (const char *)sqlite3_column_text(stmt, 0);
			const char *value = (const char *)sqlite3_column_text(stmt, 1);
			table.insert(std::make_pair(name, value));
		});
}

static bool
sticker_update_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_UPDATE];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	BindAll(stmt, value, type, uri, name);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	bool modified = ExecuteModified(stmt);

	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

static void
sticker_insert_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_INSERT];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	BindAll(stmt, type, uri, name, value);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	ExecuteCommand(stmt);
	idle_add(IDLE_STICKER);
}

void
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value)
{
	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(value != nullptr);

	if (StringIsEmpty(name))
		return;

	if (!sticker_update_value(type, uri, name, value))
		sticker_insert_value(type, uri, name, value);
}

bool
sticker_delete(const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	BindAll(stmt, type, uri);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	bool modified = ExecuteModified(stmt);
	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

bool
sticker_delete_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE_VALUE];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	BindAll(stmt, type, uri, name);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	bool modified = ExecuteModified(stmt);
	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

void
sticker_free(Sticker *sticker)
{
	delete sticker;
}

const char *
sticker_get_value(const Sticker &sticker, const char *name) noexcept
{
	auto i = sticker.table.find(name);
	if (i == sticker.table.end())
		return nullptr;

	return i->second.c_str();
}

void
sticker_foreach(const Sticker &sticker,
		void (*func)(const char *name, const char *value,
			     void *user_data),
		void *user_data)
{
	for (const auto &i : sticker.table)
		func(i.first.c_str(), i.second.c_str(), user_data);
}

Sticker *
sticker_load(const char *type, const char *uri)
{
	Sticker s;

	sticker_list_values(s.table, type, uri);

	if (s.table.empty())
		/* don't return empty sticker objects */
		return nullptr;

	return new Sticker(std::move(s));
}

static sqlite3_stmt *
BindFind(const char *type, const char *base_uri, const char *name,
	 StickerOperator op, const char *value)
{
	assert(type != nullptr);
	assert(name != nullptr);

	if (base_uri == nullptr)
		base_uri = "";

	switch (op) {
	case StickerOperator::EXISTS:
		BindAll(sticker_stmt[STICKER_SQL_FIND], type, base_uri, name);
		return sticker_stmt[STICKER_SQL_FIND];

	case StickerOperator::EQUALS:
		BindAll(sticker_stmt[STICKER_SQL_FIND_VALUE],
			type, base_uri, name, value);
		return sticker_stmt[STICKER_SQL_FIND_VALUE];

	case StickerOperator::LESS_THAN:
		BindAll(sticker_stmt[STICKER_SQL_FIND_LT],
			type, base_uri, name, value);
		return sticker_stmt[STICKER_SQL_FIND_LT];

	case StickerOperator::GREATER_THAN:
		BindAll(sticker_stmt[STICKER_SQL_FIND_GT],
			type, base_uri, name, value);
		return sticker_stmt[STICKER_SQL_FIND_GT];
	}

	assert(false);
	gcc_unreachable();
}

void
sticker_find(const char *type, const char *base_uri, const char *name,
	     StickerOperator op, const char *value,
	     void (*func)(const char *uri, const char *value,
			  void *user_data),
	     void *user_data)
{
	assert(func != nullptr);
	assert(sticker_enabled());

	sqlite3_stmt *const stmt = BindFind(type, base_uri, name, op, value);
	assert(stmt != nullptr);

	AtScopeExit(stmt) {
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	};

	ExecuteForEach(stmt, [stmt, func, user_data](){
			func((const char*)sqlite3_column_text(stmt, 0),
			     (const char*)sqlite3_column_text(stmt, 1),
			     user_data);
		});
}
