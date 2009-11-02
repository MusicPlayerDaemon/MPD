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

#ifndef MPD_UPDATE_INTERNAL_H
#define MPD_UPDATE_INTERNAL_H

#include <stdbool.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "update"

struct stat;
struct song;
struct directory;

unsigned
update_queue_push(const char *path, bool discard, unsigned base);

char *
update_queue_shift(bool *discard_r);

void
update_walk_global_init(void);

void
update_walk_global_finish(void);

/**
 * Returns true if the database was modified.
 */
bool
update_walk(const char *path, bool discard);

void
update_remove_global_init(void);

void
update_remove_global_finish(void);

/**
 * Sends a signal to the main thread which will in turn remove the
 * song: from the sticker database and from the playlist.  This
 * serialized access is implemented to avoid excessive locking.
 */
void
update_remove_song(const struct song *song);

#endif
