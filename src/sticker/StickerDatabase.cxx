/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "config.h"
#include "StickerDatabase.hxx"
#include "lib/sqlite/Domain.hxx"
#include "lib/sqlite/Util.hxx"
#include "fs/Path.hxx"
#include "Idle.hxx"
#include "util/Error.hxx"
#include "util/Macros.hxx"
#include "util/StringCompare.hxx"

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
sticker_prepare(const char *sql, Error &error)
{
	sqlite3_stmt *stmt;
	int ret = sqlite3_prepare_v2(sticker_db, sql, -1, &stmt, nullptr);
	if (ret != SQLITE_OK) {
		error.Format(sqlite_domain, ret,
			     "sqlite3_prepare_v2() failed: %s",
			     sqlite3_errmsg(sticker_db));
		return nullptr;
	}

	return stmt;
}

bool
sticker_global_init(Path path, Error &error)
{
	assert(!path.IsNull());

	int ret;

	/* open/create the sqlite database */

	ret = sqlite3_open(path.c_str(), &sticker_db);
	if (ret != SQLITE_OK) {
		const std::string utf8 = path.ToUTF8();
		error.Format(sqlite_domain, ret,
			     "Failed to open sqlite database '%s': %s",
			     utf8.c_str(), sqlite3_errmsg(sticker_db));
		return false;
	}

	/* create the table and index */

	ret = sqlite3_exec(sticker_db, sticker_sql_create,
			   nullptr, nullptr, nullptr);
	if (ret != SQLITE_OK) {
		error.Format(sqlite_domain, ret,
			     "Failed to create sticker table: %s",
			     sqlite3_errmsg(sticker_db));
		return false;
	}

	/* prepare the statements we're going to use */

	for (unsigned i = 0; i < ARRAY_SIZE(sticker_sql); ++i) {
		assert(sticker_sql[i] != nullptr);

		sticker_stmt[i] = sticker_prepare(sticker_sql[i], error);
		if (sticker_stmt[i] == nullptr)
			return false;
	}

	return true;
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
sticker_enabled()
{
	return sticker_db != nullptr;
}

std::string
sticker_load_value(const char *type, const char *uri, const char *name,
		   Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_GET];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);

	if (StringIsEmpty(name))
		return std::string();

	if (!BindAll(error, stmt, type, uri, name))
		return std::string();

	std::string value;
	if (ExecuteRow(stmt, error))
		value = (const char*)sqlite3_column_text(stmt, 0);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return value;
}

static bool
sticker_list_values(std::map<std::string, std::string> &table,
		    const char *type, const char *uri,
		    Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_LIST];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(sticker_enabled());

	if (!BindAll(error, stmt, type, uri))
		return false;

	const bool success = ExecuteForEach(stmt, error, [stmt, &table](){
			const char *name = (const char *)sqlite3_column_text(stmt, 0);
			const char *value = (const char *)sqlite3_column_text(stmt, 1);
			table.insert(std::make_pair(name, value));
		});

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return success;
}

static bool
sticker_update_value(const char *type, const char *uri,
		     const char *name, const char *value,
		     Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_UPDATE];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	if (!BindAll(error, stmt, value, type, uri, name))
		return false;

	bool modified = ExecuteModified(stmt, error);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

static bool
sticker_insert_value(const char *type, const char *uri,
		     const char *name, const char *value,
		     Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_INSERT];

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	if (!BindAll(error, stmt, type, uri, name, value))
		return false;

	bool success = ExecuteCommand(stmt, error);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	if (success)
		idle_add(IDLE_STICKER);
	return success;
}

bool
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value,
		    Error &error)
{
	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(value != nullptr);

	if (StringIsEmpty(name))
		return false;

	return sticker_update_value(type, uri, name, value, error) ||
		sticker_insert_value(type, uri, name, value, error);
}

bool
sticker_delete(const char *type, const char *uri, Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	if (!BindAll(error, stmt, type, uri))
		return false;

	bool modified = ExecuteModified(stmt, error);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	if (modified)
		idle_add(IDLE_STICKER);
	return modified;
}

bool
sticker_delete_value(const char *type, const char *uri, const char *name,
		     Error &error)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE_VALUE];

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	if (!BindAll(error, stmt, type, uri, name))
		return false;

	bool modified = ExecuteModified(stmt, error);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

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
sticker_get_value(const Sticker &sticker, const char *name)
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
sticker_load(const char *type, const char *uri, Error &error)
{
	Sticker s;

	if (!sticker_list_values(s.table, type, uri, error))
		return nullptr;

	if (s.table.empty())
		/* don't return empty sticker objects */
		return nullptr;

	return new Sticker(std::move(s));
}

static sqlite3_stmt *
BindFind(const char *type, const char *base_uri, const char *name,
	 StickerOperator op, const char *value,
	 Error &error)
{
	assert(type != nullptr);
	assert(name != nullptr);

	if (base_uri == nullptr)
		base_uri = "";

	switch (op) {
	case StickerOperator::EXISTS:
		return BindAllOrNull(error, sticker_stmt[STICKER_SQL_FIND],
				     type, base_uri, name);

	case StickerOperator::EQUALS:
		return BindAllOrNull(error,
				     sticker_stmt[STICKER_SQL_FIND_VALUE],
				     type, base_uri, name, value);

	case StickerOperator::LESS_THAN:
		return BindAllOrNull(error,
				     sticker_stmt[STICKER_SQL_FIND_LT],
				     type, base_uri, name, value);

	case StickerOperator::GREATER_THAN:
		return BindAllOrNull(error,
				     sticker_stmt[STICKER_SQL_FIND_GT],
				     type, base_uri, name, value);
	}

	assert(false);
	gcc_unreachable();
}

bool
sticker_find(const char *type, const char *base_uri, const char *name,
	     StickerOperator op, const char *value,
	     void (*func)(const char *uri, const char *value,
			  void *user_data),
	     void *user_data,
	     Error &error)
{
	assert(func != nullptr);
	assert(sticker_enabled());

	sqlite3_stmt *const stmt = BindFind(type, base_uri, name, op, value,
					    error);
	if (stmt == nullptr)
		return false;

	const bool success = ExecuteForEach(stmt, error,
					    [stmt, func, user_data](){
			func((const char*)sqlite3_column_text(stmt, 0),
			     (const char*)sqlite3_column_text(stmt, 1),
			     user_data);
		});

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return success;
}
