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

#ifndef MPD_TAG_APE_H
#define MPD_TAG_APE_H

#include "tag_table.h"

#include <stdbool.h>

struct tag_handler;

extern const struct tag_table ape_tags[];

/**
 * Scan the APE tags of a file.
 *
 * @param path_fs the path of the file in filesystem encoding
 */
bool
tag_ape_scan2(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx);

#endif
