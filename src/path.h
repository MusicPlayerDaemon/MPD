/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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

#ifndef MPD_PATH_H
#define MPD_PATH_H

#include "os_compat.h"

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

char *fs_charset_to_utf8(char *dst, const char *str);

char *utf8_to_fs_charset(char *dst, const char *str);

void path_set_fs_charset(const char *charset);

const char *path_get_fs_charset(void);

/*
 * pfx_dir - sets dst="$pfx/$path" and returns a pointer to path inside * dst
 * this will unconditionally put a '/' between pfx and path unlike
 * the static pfx_path() function in path.c
 * dst is assumed to be MAXPATHLEN in size
 * dst can point to the same location as path, but not pfx, which makes
 * this better than sprintf(3) in some cases
 */
char *pfx_dir(char *dst,
              const char *path, const size_t path_len,
              const char *pfx, const size_t pfx_len);

#endif
