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
#include "util/Compiler.h"

#include <string>

class Path;
struct Sticker;

/**
 * Opens the sticker database.
 *
 * Throws std::runtime_error on error.
 */
void
sticker_global_init(Path path);

/**
 * Close the sticker database.
 */
void
sticker_global_finish();

/**
 * Returns true if the sticker database is configured and available.
 */
gcc_const
bool
sticker_enabled() noexcept;

/**
 * Returns one value from an object's sticker record.  Returns an
 * empty string if the value doesn't exist.
 *
 * Throws #SqliteError on error.
 */
std::string
sticker_load_value(const char *type, const char *uri, const char *name);

/**
 * Sets a sticker value in the specified object.  Overwrites existing
 * values.
 *
 * Throws #SqliteError on error.
 */
void
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value);

/**
 * Deletes a sticker from the database.  All sticker values of the
 * specified object are deleted.
 *
 * Throws #SqliteError on error.
 */
bool
sticker_delete(const char *type, const char *uri);

/**
 * Deletes a sticker value.  Fails if no sticker with this name
 * exists.
 *
 * Throws #SqliteError on error.
 */
bool
sticker_delete_value(const char *type, const char *uri, const char *name);

/**
 * Frees resources held by the sticker object.
 *
 * @param sticker the sticker object to be freed
 */
void
sticker_free(Sticker *sticker);

/**
 * Determines a single value in a sticker.
 *
 * @param sticker the sticker object
 * @param name the name of the sticker
 * @return the sticker value, or nullptr if none was found
 */
gcc_pure
const char *
sticker_get_value(const Sticker &sticker, const char *name) noexcept;

/**
 * Iterates over all sticker items in a sticker.
 *
 * @param sticker the sticker object
 * @param func a callback function
 * @param user_data an opaque pointer for the callback function
 */
void
sticker_foreach(const Sticker &sticker,
		void (*func)(const char *name, const char *value,
			     void *user_data),
		void *user_data);

/**
 * Loads the sticker for the specified resource.
 *
 * Throws #SqliteError on error.
 *
 * @param type the resource type, e.g. "song"
 * @param uri the URI of the resource, e.g. the song path
 * @return a sticker object, or nullptr if there is no sticker
 */
Sticker *
sticker_load(const char *type, const char *uri);

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
void
sticker_find(const char *type, const char *base_uri, const char *name,
	     StickerOperator op, const char *value,
	     void (*func)(const char *uri, const char *value,
			  void *user_data),
	     void *user_data);

#endif
