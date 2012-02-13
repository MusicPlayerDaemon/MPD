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

#ifndef MPD_PLAYLIST_VECTOR_H
#define MPD_PLAYLIST_VECTOR_H

#include "util/list.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

#define playlist_vector_for_each(pos, head) \
	list_for_each_entry(pos, head, siblings)

#define playlist_vector_for_each_safe(pos, n, head) \
	list_for_each_entry_safe(pos, n, head, siblings)

/**
 * A directory entry pointing to a playlist file.
 */
struct playlist_metadata {
	struct list_head siblings;

	/**
	 * The UTF-8 encoded name of the playlist file.
	 */
	char *name;

	time_t mtime;
};

void
playlist_vector_deinit(struct list_head *pv);

/**
 * Caller must lock the #db_mutex.
 */
struct playlist_metadata *
playlist_vector_find(struct list_head *pv, const char *name);

/**
 * Caller must lock the #db_mutex.
 */
void
playlist_vector_add(struct list_head *pv,
		    const char *name, time_t mtime);

/**
 * Caller must lock the #db_mutex.
 *
 * @return true if the vector or one of its items was modified
 */
bool
playlist_vector_update_or_add(struct list_head *pv,
			      const char *name, time_t mtime);

/**
 * Caller must lock the #db_mutex.
 */
bool
playlist_vector_remove(struct list_head *pv, const char *name);

#endif /* SONGVEC_H */
