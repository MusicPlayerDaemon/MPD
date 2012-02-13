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

#include "config.h"
#include "playlist_vector.h"
#include "db_lock.h"

#include <assert.h>
#include <string.h>
#include <glib.h>

static struct playlist_metadata *
playlist_metadata_new(const char *name, time_t mtime)
{
	assert(name != NULL);

	struct playlist_metadata *pm = g_slice_new(struct playlist_metadata);
	pm->name = g_strdup(name);
	pm->mtime = mtime;
	return pm;
}

static void
playlist_metadata_free(struct playlist_metadata *pm)
{
	assert(pm != NULL);
	assert(pm->name != NULL);

	g_free(pm->name);
	g_slice_free(struct playlist_metadata, pm);
}

void
playlist_vector_deinit(struct list_head *pv)
{
	assert(pv != NULL);

	struct playlist_metadata *pm, *n;
	playlist_vector_for_each_safe(pm, n, pv)
		playlist_metadata_free(pm);
}

struct playlist_metadata *
playlist_vector_find(struct list_head *pv, const char *name)
{
	assert(holding_db_lock());
	assert(pv != NULL);
	assert(name != NULL);

	struct playlist_metadata *pm;
	playlist_vector_for_each(pm, pv)
		if (strcmp(pm->name, name) == 0)
			return pm;

	return NULL;
}

void
playlist_vector_add(struct list_head *pv,
		    const char *name, time_t mtime)
{
	assert(holding_db_lock());

	struct playlist_metadata *pm = playlist_metadata_new(name, mtime);
	list_add_tail(&pm->siblings, pv);
}

bool
playlist_vector_update_or_add(struct list_head *pv,
			      const char *name, time_t mtime)
{
	assert(holding_db_lock());

	struct playlist_metadata *pm = playlist_vector_find(pv, name);
	if (pm != NULL) {
		if (mtime == pm->mtime)
			return false;

		pm->mtime = mtime;
	} else
		playlist_vector_add(pv, name, mtime);

	return true;
}

bool
playlist_vector_remove(struct list_head *pv, const char *name)
{
	assert(holding_db_lock());

	struct playlist_metadata *pm = playlist_vector_find(pv, name);
	if (pm == NULL)
		return false;

	list_del(&pm->siblings);
	playlist_metadata_free(pm);
	return true;
}
