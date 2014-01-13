/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "fs/Path.hxx"
#include "Idle.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/Macros.hxx"
#include "Log.hxx"

#include <string>
#include <map>

#include <sqlite3.h>
#include <assert.h>

#if SQLITE_VERSION_NUMBER < 3003009
#define sqlite3_prepare_v2 sqlite3_prepare
#endif

struct sticker {
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

static constexpr Domain sticker_domain("sticker");

static void
LogError(sqlite3 *db, const char *msg)
{
	FormatError(sticker_domain, "%s: %s", msg, sqlite3_errmsg(db));
}

static sqlite3_stmt *
sticker_prepare(const char *sql, Error &error)
{
	int ret;
	sqlite3_stmt *stmt;

	ret = sqlite3_prepare_v2(sticker_db, sql, -1, &stmt, nullptr);
	if (ret != SQLITE_OK) {
		error.Format(sticker_domain, ret,
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
		error.Format(sticker_domain, ret,
			    "Failed to open sqlite database '%s': %s",
			    utf8.c_str(), sqlite3_errmsg(sticker_db));
		return false;
	}

	/* create the table and index */

	ret = sqlite3_exec(sticker_db, sticker_sql_create,
			   nullptr, nullptr, nullptr);
	if (ret != SQLITE_OK) {
		error.Format(sticker_domain, ret,
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
sticker_global_finish(void)
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
sticker_enabled(void)
{
	return sticker_db != nullptr;
}

std::string
sticker_load_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_GET];
	int ret;

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);

	if (*name == 0)
		return std::string();

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return std::string();
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return std::string();
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return std::string();
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	std::string value;
	if (ret == SQLITE_ROW) {
		/* record found */
		value = (const char*)sqlite3_column_text(stmt, 0);
	} else if (ret == SQLITE_DONE) {
		/* no record found */
	} else {
		/* error */
		LogError(sticker_db, "sqlite3_step() failed");
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return value;
}

static bool
sticker_list_values(std::map<std::string, std::string> &table,
		    const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_LIST];
	int ret;

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
		switch (ret) {
			const char *name, *value;

		case SQLITE_ROW:
			name = (const char*)sqlite3_column_text(stmt, 0);
			value = (const char*)sqlite3_column_text(stmt, 1);

			table.insert(std::make_pair(name, value));
			break;
		case SQLITE_DONE:
			break;
		case SQLITE_BUSY:
			/* no op */
			break;
		default:
			LogError(sticker_db, "sqlite3_step() failed");
			return false;
		}
	} while (ret != SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return true;
}

static bool
sticker_update_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_UPDATE];
	int ret;

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, value, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 4, name, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		LogError(sticker_db, "sqlite3_step() failed");
		return false;
	}

	ret = sqlite3_changes(sticker_db);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return ret > 0;
}

static bool
sticker_insert_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_INSERT];
	int ret;

	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(*name != 0);
	assert(value != nullptr);

	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 4, value, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		LogError(sticker_db, "sqlite3_step() failed");
		return false;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);


	idle_add(IDLE_STICKER);
	return true;
}

bool
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value)
{
	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);
	assert(name != nullptr);
	assert(value != nullptr);

	if (*name == 0)
		return false;

	return sticker_update_value(type, uri, name, value) ||
		sticker_insert_value(type, uri, name, value);
}

bool
sticker_delete(const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE];
	int ret;

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		LogError(sticker_db, "sqlite3_step() failed");
		return false;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return true;
}

bool
sticker_delete_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE_VALUE];
	int ret;

	assert(sticker_enabled());
	assert(type != nullptr);
	assert(uri != nullptr);

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		LogError(sticker_db, "sqlite3_step() failed");
		return false;
	}

	ret = sqlite3_changes(sticker_db);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return ret > 0;
}

void
sticker_free(struct sticker *sticker)
{
	delete sticker;
}

const char *
sticker_get_value(const struct sticker &sticker, const char *name)
{
	auto i = sticker.table.find(name);
	if (i == sticker.table.end())
		return nullptr;

	return i->second.c_str();
}

void
sticker_foreach(const sticker &sticker,
		void (*func)(const char *name, const char *value,
			     void *user_data),
		void *user_data)
{
	for (const auto &i : sticker.table)
		func(i.first.c_str(), i.second.c_str(), user_data);
}

struct sticker *
sticker_load(const char *type, const char *uri)
{
	sticker s;

	if (!sticker_list_values(s.table, type, uri))
		return nullptr;

	if (s.table.empty())
		/* don't return empty sticker objects */
		return nullptr;

	return new sticker(std::move(s));
}

bool
sticker_find(const char *type, const char *base_uri, const char *name,
	     void (*func)(const char *uri, const char *value,
			  void *user_data),
	     void *user_data)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_FIND];
	int ret;

	assert(type != nullptr);
	assert(name != nullptr);
	assert(func != nullptr);
	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	if (base_uri == nullptr)
		base_uri = "";

	ret = sqlite3_bind_text(stmt, 2, base_uri, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, nullptr);
	if (ret != SQLITE_OK) {
		LogError(sticker_db, "sqlite3_bind_text() failed");
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
		switch (ret) {
		case SQLITE_ROW:
			func((const char*)sqlite3_column_text(stmt, 0),
			     (const char*)sqlite3_column_text(stmt, 1),
			     user_data);
			break;
		case SQLITE_DONE:
			break;
		case SQLITE_BUSY:
			/* no op */
			break;
		default:
			LogError(sticker_db, "sqlite3_step() failed");
			return false;
		}
	} while (ret != SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return true;
}
