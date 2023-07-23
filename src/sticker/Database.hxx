// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This is the sticker database library.  It is the backend of all the
 * sticker code in MPD.
 *
 * "Stickers" are pieces of information attached to existing MPD
 * objects (e.g. song files, directories, albums).  Clients can create
 * arbitrary name/value pairs.  MPD itself does not assume any special
 * meaning in them.
 *
 * The goal is to allow clients to share additional (possibly dynamic)
 * information about songs, which is neither stored on the client (not
 * available to other clients), nor stored in the song files (MPD has
 * no write access).
 *
 * Client developers should create a standard for common sticker
 * names, to ensure interoperability.
 *
 * Examples: song ratings; statistics; deferred tag writes; lyrics;
 * ...
 *
 */

#ifndef MPD_STICKER_DATABASE_HXX
#define MPD_STICKER_DATABASE_HXX

#include "Match.hxx"
#include "lib/sqlite/Database.hxx"

#include <sqlite3.h>

#include <map>
#include <string>

class Path;
struct Sticker;

class StickerDatabase {
	enum SQL {
		  SQL_GET,
		  SQL_LIST,
		  SQL_UPDATE,
		  SQL_INSERT,
		  SQL_DELETE,
		  SQL_DELETE_VALUE,
		  SQL_FIND,
		  SQL_FIND_VALUE,
		  SQL_FIND_LT,
		  SQL_FIND_GT,

		  SQL_COUNT
	};

	Sqlite::Database db;
	sqlite3_stmt *stmt[SQL_COUNT];

public:
	/**
	 * Opens the sticker database.
	 *
	 * Throws on error.
	 */
	StickerDatabase(Path path);
	~StickerDatabase() noexcept;

	StickerDatabase(StickerDatabase &&) noexcept = default;
	StickerDatabase &operator=(StickerDatabase &&) noexcept = default;

	/**
	 * Returns one value from an object's sticker record.  Returns an
	 * empty string if the value doesn't exist.
	 *
	 * Throws #SqliteError on error.
	 */
	std::string LoadValue(const char *type, const char *uri,
			      const char *name);

	/**
	 * Sets a sticker value in the specified object.  Overwrites existing
	 * values.
	 *
	 * Throws #SqliteError on error.
	 */
	void StoreValue(const char *type, const char *uri,
			const char *name, const char *value);

	/**
	 * Deletes a sticker from the database.  All sticker values of the
	 * specified object are deleted.
	 *
	 * Throws #SqliteError on error.
	 */
	bool Delete(const char *type, const char *uri);

	/**
	 * Deletes a sticker value.  Fails if no sticker with this name
	 * exists.
	 *
	 * Throws #SqliteError on error.
	 */
	bool DeleteValue(const char *type, const char *uri, const char *name);

	/**
	 * Loads the sticker for the specified resource.
	 *
	 * Throws #SqliteError on error.
	 *
	 * @param type the resource type, e.g. "song"
	 * @param uri the URI of the resource, e.g. the song path
	 * @return a sticker object
	 */
	Sticker Load(const char *type, const char *uri);

	/**
	 * Finds stickers with the specified name below the specified URI.
	 *
	 * @param type the resource type, e.g. "song"
	 * @param base_uri the URI prefix of the resources, or nullptr if all
	 * resources should be searched
	 * @param name the name of the sticker
	 * @param op the comparison operator
	 * @param value the operand
	 */
	void Find(const char *type, const char *base_uri, const char *name,
		  StickerOperator op, const char *value,
		  void (*func)(const char *uri, const char *value,
			       void *user_data),
		  void *user_data);

private:
	void ListValues(std::map<std::string, std::string, std::less<>> &table,
			const char *type, const char *uri);

	bool UpdateValue(const char *type, const char *uri,
			 const char *name, const char *value);

	void InsertValue(const char *type, const char *uri,
			 const char *name, const char *value);

	sqlite3_stmt *BindFind(const char *type, const char *base_uri,
			       const char *name,
			       StickerOperator op, const char *value);
};

#endif
