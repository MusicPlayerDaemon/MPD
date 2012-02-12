/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#ifndef MPD_UPDATE_IO_H
#define MPD_UPDATE_IO_H

#include "check.h"

#include <stdbool.h>
#include <sys/stat.h>

struct directory;

int
stat_directory(const struct directory *directory, struct stat *st);

int
stat_directory_child(const struct directory *parent, const char *name,
		     struct stat *st);

bool
directory_exists(const struct directory *directory);

bool
directory_child_is_regular(const struct directory *directory,
			   const char *name_utf8);

/**
 * Checks if the given permissions on the mapped file are given.
 */
bool
directory_child_access(const struct directory *directory,
		       const char *name, int mode);

#endif
