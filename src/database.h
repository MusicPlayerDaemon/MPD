/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_DATABASE_H
#define MPD_DATABASE_H

#include "gcc.h"

#include <glib.h>

#include <sys/time.h>
#include <stdbool.h>

struct config_param;
struct directory;
struct db_selection;
struct db_visitor;

/**
 * Initialize the database library.
 *
 * @param path the absolute path of the database file
 */
bool
db_init(const struct config_param *path, GError **error_r);

void
db_finish(void);

/**
 * Returns the root directory object.  Returns NULL if there is no
 * configured music directory.
 */
G_GNUC_PURE
struct directory *
db_get_root(void);

/**
 * Caller must lock the #db_mutex.
 */
gcc_nonnull(1)
G_GNUC_PURE
struct directory *
db_get_directory(const char *name);

gcc_nonnull(1)
G_GNUC_PURE
struct song *
db_get_song(const char *file);

gcc_nonnull(1,2)
bool
db_visit(const struct db_selection *selection,
	 const struct db_visitor *visitor, void *ctx,
	 GError **error_r);

gcc_nonnull(1,2)
bool
db_walk(const char *uri,
	const struct db_visitor *visitor, void *ctx,
	GError **error_r);

bool
db_save(GError **error_r);

bool
db_load(GError **error);

G_GNUC_PURE
time_t
db_get_mtime(void);

/**
 * Returns true if there is a valid database file on the disk.
 */
G_GNUC_PURE
static inline bool
db_exists(void)
{
	/* mtime is set only if the database file was loaded or saved
	   successfully */
	return db_get_mtime() > 0;
}

#endif
