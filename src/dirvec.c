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

#include "config.h"
#include "dirvec.h"
#include "directory.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static GMutex *nr_lock = NULL;

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

void dirvec_init(void)
{
	g_assert(nr_lock == NULL);
	nr_lock = g_mutex_new();
}

void dirvec_deinit(void)
{
	g_assert(nr_lock != NULL);
	g_mutex_free(nr_lock);
	nr_lock = NULL;
}

void dirvec_sort(struct dirvec *dv)
{
	g_mutex_lock(nr_lock);
	qsort(dv->base, dv->nr, sizeof(struct directory *), dirvec_cmp);
	g_mutex_unlock(nr_lock);
}

struct directory *dirvec_find(const struct dirvec *dv, const char *path)
{
	char *base;
	int i;
	struct directory *ret = NULL;

	base = g_path_get_basename(path);

	g_mutex_lock(nr_lock);
	for (i = dv->nr; --i >= 0; )
		if (!strcmp(directory_get_name(dv->base[i]), base)) {
			ret = dv->base[i];
			break;
		}
	g_mutex_unlock(nr_lock);

	g_free(base);
	return ret;
}

int dirvec_delete(struct dirvec *dv, struct directory *del)
{
	size_t i;

	g_mutex_lock(nr_lock);
	for (i = 0; i < dv->nr; ++i) {
		if (dv->base[i] != del)
			continue;
		/* we _don't_ call directory_free() here */
		if (!--dv->nr) {
			g_mutex_unlock(nr_lock);
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
	g_mutex_unlock(nr_lock);

	return i;
}

void dirvec_add(struct dirvec *dv, struct directory *add)
{
	g_mutex_lock(nr_lock);
	++dv->nr;
	dv->base = g_realloc(dv->base, dv_size(dv));
	dv->base[dv->nr - 1] = add;
	g_mutex_unlock(nr_lock);
}

void dirvec_destroy(struct dirvec *dv)
{
	g_mutex_lock(nr_lock);
	dv->nr = 0;
	g_mutex_unlock(nr_lock);
	if (dv->base) {
		g_free(dv->base);
		dv->base = NULL;
	}
}

int dirvec_for_each(const struct dirvec *dv,
                    int (*fn)(struct directory *, void *), void *arg)
{
	size_t i;
	size_t prev_nr;

	g_mutex_lock(nr_lock);
	for (i = 0; i < dv->nr; ) {
		struct directory *dir = dv->base[i];

		assert(dir);
		prev_nr = dv->nr;
		g_mutex_unlock(nr_lock);
		if (fn(dir, arg) < 0)
			return -1;
		g_mutex_lock(nr_lock); /* dv->nr may change in fn() */
		if (prev_nr == dv->nr)
			++i;
	}
	g_mutex_unlock(nr_lock);

	return 0;
}
