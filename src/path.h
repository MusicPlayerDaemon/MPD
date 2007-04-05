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

#ifndef PATH_H
#define PATH_H

#include "../config.h"

#include <sys/param.h>

extern const char *musicDir;

void initPaths(void);

void finishPaths(void);

/* utf8ToFsCharset() and fsCharsetToUtf8()
 * Each returns a static pointer to a dynamically allocated buffer
 * which means:
 * - Do not manually free the return value of these functions, it'll be
 *   automatically freed the next time it is called.
 * - They are not reentrant, xstrdup the return value immediately if
 *   you expect to call one of these functions again, but still need the
 *   previous result.
 * - The static pointer is unique to each function.
 */
char *utf8ToFsCharset(char *str);
char *fsCharsetToUtf8(char *str);

void setFsCharset(char *charset);

char *getFsCharset(void);

/* relative music path to absolute music path
 * char * passed is a static variable, so don't free it
 */
char *rmp2amp(char *file);

/* static char * returned */
char *rpp2app(char *file);

/* static char * returned */
char *parentPath(char *path);

/* strips extra "///" and leading "/" and trailing "/" */
char *sanitizePathDup(char *path);

/* this is actually like strlcpy (OpenBSD), but we don't actually want to
 * blindly use it everywhere, only for paths that are OK to truncate (for
 * error reporting and such.
 * dest must be MAXPATHLEN+1 bytes large (which is standard in mpd) */
void pathcpy_trunc(char *dest, const char *src);

#endif
