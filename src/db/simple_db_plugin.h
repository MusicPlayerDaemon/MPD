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

#ifndef MPD_SIMPLE_DB_PLUGIN_H
#define MPD_SIMPLE_DB_PLUGIN_H

#include <glib.h>
#include <stdbool.h>
#include <time.h>

extern const struct db_plugin simple_db_plugin;

struct db;

G_GNUC_PURE
struct directory *
simple_db_get_root(struct db *db);

bool
simple_db_save(struct db *db, GError **error_r);

G_GNUC_PURE
time_t
simple_db_get_mtime(const struct db *db);

#endif
