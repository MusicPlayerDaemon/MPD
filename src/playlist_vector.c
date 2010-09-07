/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
playlist_vector_deinit(struct playlist_vector *pv)
{
	assert(pv != NULL);

	while (pv->head != NULL) {
		struct playlist_metadata *pm = pv->head;
		pv->head = pm->next;
		playlist_metadata_free(pm);
	}
}

static struct playlist_metadata **
playlist_vector_find_p(struct playlist_vector *pv, const char *name)
{
	assert(pv != NULL);
	assert(name != NULL);

	struct playlist_metadata **pmp = &pv->head;

	for (;;) {
		struct playlist_metadata *pm = *pmp;
		if (pm == NULL)
			return NULL;

		if (strcmp(pm->name, name) == 0)
			return pmp;

		pmp = &pm->next;
	}
}

struct playlist_metadata *
playlist_vector_find(struct playlist_vector *pv, const char *name)
{
	struct playlist_metadata **pmp = playlist_vector_find_p(pv, name);
	return pmp != NULL ? *pmp : NULL;
}

void
playlist_vector_add(struct playlist_vector *pv,
		    const char *name, time_t mtime)
{
	struct playlist_metadata *pm = playlist_metadata_new(name, mtime);
	pm->next = pv->head;
	pv->head = pm;
}

bool
playlist_vector_update_or_add(struct playlist_vector *pv,
			      const char *name, time_t mtime)
{
	struct playlist_metadata **pmp = playlist_vector_find_p(pv, name);
	if (pmp != NULL) {
		struct playlist_metadata *pm = *pmp;
		if (mtime == pm->mtime)
			return false;

		pm->mtime = mtime;
	} else
		playlist_vector_add(pv, name, mtime);

	return true;
}

bool
playlist_vector_remove(struct playlist_vector *pv, const char *name)
{
	struct playlist_metadata **pmp = playlist_vector_find_p(pv, name);
	if (pmp == NULL)
		return false;

	struct playlist_metadata *pm = *pmp;
	*pmp = pm->next;

	playlist_metadata_free(pm);
	return true;
}
