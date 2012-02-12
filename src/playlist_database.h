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

#ifndef MPD_PLAYLIST_DATABASE_H
#define MPD_PLAYLIST_DATABASE_H

#include "check.h"

#include <stdbool.h>
#include <stdio.h>
#include <glib.h>

#define PLAYLIST_META_BEGIN "playlist_begin: "

struct list_head;

void
playlist_vector_save(FILE *fp, const struct list_head *pv);

bool
playlist_metadata_load(FILE *fp, struct list_head *pv, const char *name,
		       GString *buffer, GError **error_r);

#endif
