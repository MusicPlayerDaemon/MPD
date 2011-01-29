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

#ifndef MPD_APE_H
#define MPD_APE_H

#include "check.h"

#include <stdbool.h>
#include <stddef.h>

typedef bool (*tag_ape_callback_t)(unsigned long flags, const char *key,
				   const char *value, size_t value_length,
				   void *ctx);

/**
 * Scans the APE tag values from a file.
 *
 * @param path_fs the path of the file in filesystem encoding
 * @return false if the file could not be opened or if no APE tag is
 * present
 */
bool
tag_ape_scan(const char *path_fs, tag_ape_callback_t callback, void *ctx);

#endif
