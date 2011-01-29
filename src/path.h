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

#ifndef MPD_PATH_H
#define MPD_PATH_H

#include <limits.h>

#if !defined(MPD_PATH_MAX)
#  if defined(MAXPATHLEN)
#    define MPD_PATH_MAX MAXPATHLEN
#  elif defined(PATH_MAX)
#    define MPD_PATH_MAX PATH_MAX
#  else
#    define MPD_PATH_MAX 256
#  endif
#endif

void path_global_init(void);

void path_global_finish(void);

/**
 * Converts a file name in the filesystem charset to UTF-8.  Returns
 * NULL on failure.
 */
char *
fs_charset_to_utf8(const char *path_fs);

/**
 * Converts a file name in UTF-8 to the filesystem charset.  Returns a
 * duplicate of the UTF-8 string on failure.
 */
char *
utf8_to_fs_charset(const char *path_utf8);

const char *path_get_fs_charset(void);

#endif
