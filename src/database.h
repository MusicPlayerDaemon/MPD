/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
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

#ifndef MPD_DATABASE_H
#define MPD_DATABASE_H

#include <sys/time.h>

struct directory;

void
db_init(void);

void
db_finish(void);

struct directory *
db_get_root(void);

struct directory *
db_get_directory(const char *name);

struct song *
db_get_song(const char *file);

int db_walk(const char *name,
	    int (*forEachSong)(struct song *, void *),
	    int (*forEachDir)(struct directory *, void *), void *data);

int
db_check(void);

int
db_save(void);

int
db_load(void);

time_t
db_get_mtime(void);

#endif
