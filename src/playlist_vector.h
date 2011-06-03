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

#include <stdbool.h>
#include <stddef.h>
#include <sys/time.h>

/**
 * A directory entry pointing to a playlist file.
 */
struct playlist_metadata {
	struct playlist_metadata *next;

	/**
	 * The UTF-8 encoded name of the playlist file.
	 */
	char *name;

	time_t mtime;
};

struct playlist_vector {
	struct playlist_metadata *head;
};

static inline void
playlist_vector_init(struct playlist_vector *pv)
{
	pv->head = NULL;
}

void
playlist_vector_deinit(struct playlist_vector *pv);

static inline bool
playlist_vector_is_empty(const struct playlist_vector *pv)
{
	return pv->head == NULL;
}

struct playlist_metadata *
playlist_vector_find(struct playlist_vector *pv, const char *name);

void
playlist_vector_add(struct playlist_vector *pv,
		    const char *name, time_t mtime);

/**
 * @return true if the vector or one of its items was modified
 */
bool
playlist_vector_update_or_add(struct playlist_vector *pv,
			      const char *name, time_t mtime);

bool
playlist_vector_remove(struct playlist_vector *pv, const char *name);

#endif /* SONGVEC_H */
