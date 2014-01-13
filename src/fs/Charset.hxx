/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_FS_CHARSET_HXX
#define MPD_FS_CHARSET_HXX

#include "check.h"
#include "Compiler.h"

#include <string>

/**
 * Gets file system character set name.
 */
gcc_const
const char *
GetFSCharset();

void
SetFSCharset(const char *charset);

/**
 * Convert the path to UTF-8.
 * Returns empty string on error.
 */
gcc_pure gcc_nonnull_all
std::string
PathToUTF8(const char *path_fs);

gcc_malloc gcc_nonnull_all
char *
PathFromUTF8(const char *path_utf8);

#endif
