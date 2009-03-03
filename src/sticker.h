/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#ifndef STICKER_H
#define STICKER_H

#include <glib.h>

#include <stdbool.h>

/**
 * Opens the sticker database (if path is not NULL).
 */
void
sticker_global_init(const char *path);

/**
 * Close the sticker database.
 */
void
sticker_global_finish(void);

/**
 * Returns true if the sticker database is configured and available.
 */
bool
sticker_enabled(void);

/**
 * Populates a GList with GPtrArrays of sticker names and values from
 * an object's sticker record.  The caller must free each GPtrArray
 * element of the returned list with g_ptr_array_free(), as well as
 * the returned GList with g_list_free().
 */
GList *
sticker_list_values(const char *type, const char *uri);

/**
 * Returns one value from an object's sticker record.  The caller must
 * free the return value with g_free().
 */
char *
sticker_load_value(const char *type, const char *uri, const char *name);

/**
 * Sets a sticker value in the specified object.  Overwrites existing
 * values.
 */
bool
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value);

/**
 * Deletes a sticker from the database.  All sticker values of the
 * specified object are deleted.
 */
bool
sticker_delete(const char *type, const char *uri);

#endif
