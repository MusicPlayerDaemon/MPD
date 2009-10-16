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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * The .mpdignore backend code.
 *
 */

#ifndef MPD_EXCLUDE_H
#define MPD_EXCLUDE_H

#include <glib.h>

#include <stdbool.h>

/**
 * Loads and parses a .mpdignore file.
 */
GSList *
exclude_list_load(const char *path_fs);

/**
 * Frees a list returned by exclude_list_load().
 */
void
exclude_list_free(GSList *list);

/**
 * Checks whether one of the patterns in the .mpdignore file matches
 * the specified file name.
 */
bool
exclude_list_check(GSList *list, const char *name_fs);

#endif
