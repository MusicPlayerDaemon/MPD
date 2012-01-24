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
#include "dirvec.h"
#include "directory.h"
#include "db_lock.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static size_t dv_size(const struct dirvec *dv)
{
	return dv->nr * sizeof(struct directory *);
}

/* Only used for sorting/searching a dirvec, not general purpose compares */
static int dirvec_cmp(const void *d1, const void *d2)
{
	const struct directory *a = ((const struct directory * const *)d1)[0];
	const struct directory *b = ((const struct directory * const *)d2)[0];
	return g_utf8_collate(a->path, b->path);
}

void dirvec_sort(struct dirvec *dv)
{
	db_lock();
	qsort(dv->base, dv->nr, sizeof(struct directory *), dirvec_cmp);
	db_unlock();
}

struct directory *dirvec_find(const struct dirvec *dv, const char *path)
{
	int i;

	db_lock();
	for (i = dv->nr; --i >= 0; )
		if (!strcmp(directory_get_name(dv->base[i]), path))
			return dv->base[i];
	db_unlock();

	return NULL;
}

int dirvec_delete(struct dirvec *dv, struct directory *del)
{
	size_t i;

	db_lock();
	for (i = 0; i < dv->nr; ++i) {
		if (dv->base[i] != del)
			continue;
		/* we _don't_ call directory_free() here */
		if (!--dv->nr) {
			db_unlock();
			g_free(dv->base);
			dv->base = NULL;
			return i;
		} else {
			memmove(&dv->base[i], &dv->base[i + 1],
				(dv->nr - i) * sizeof(struct directory *));
			dv->base = g_realloc(dv->base, dv_size(dv));
		}
		break;
	}
	db_unlock();

	return i;
}

void dirvec_add(struct dirvec *dv, struct directory *add)
{
	db_lock();
	++dv->nr;
	dv->base = g_realloc(dv->base, dv_size(dv));
	dv->base[dv->nr - 1] = add;
	db_unlock();
}

void dirvec_destroy(struct dirvec *dv)
{
	db_lock();
	dv->nr = 0;
	db_unlock();
	if (dv->base) {
		g_free(dv->base);
		dv->base = NULL;
	}
}
