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
#include "songvec.h"
#include "song.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static GMutex *nr_lock = NULL;

static const char *
tag_get_value_checked(const struct tag *tag, enum tag_type type)
{
	return tag != NULL
		? tag_get_value(tag, type)
		: NULL;
}

static int
compare_utf8_string(const char *a, const char *b)
{
	if (a == NULL)
		return b == NULL ? 0 : -1;

	if (b == NULL)
		return 1;

	return g_utf8_collate(a, b);
}

/**
 * Compare two string tag values, ignoring case.  Either one may be
 * NULL.
 */
static int
compare_string_tag_item(const struct tag *a, const struct tag *b,
			enum tag_type type)
{
	return compare_utf8_string(tag_get_value_checked(a, type),
				   tag_get_value_checked(b, type));
}

/**
 * Compare two tag values which should contain an integer value
 * (e.g. disc or track number).  Either one may be NULL.
 */
static int
compare_number_string(const char *a, const char *b)
{
	long ai = a == NULL ? 0 : strtol(a, NULL, 10);
	long bi = b == NULL ? 0 : strtol(b, NULL, 10);

	if (ai <= 0)
		return bi <= 0 ? 0 : -1;

	if (bi <= 0)
		return 1;

	return ai - bi;
}

static int
compare_tag_item(const struct tag *a, const struct tag *b, enum tag_type type)
{
	return compare_number_string(tag_get_value_checked(a, type),
				     tag_get_value_checked(b, type));
}

/* Only used for sorting/searchin a songvec, not general purpose compares */
static int songvec_cmp(const void *s1, const void *s2)
{
	const struct song *a = ((const struct song * const *)s1)[0];
	const struct song *b = ((const struct song * const *)s2)[0];
	int ret;

	/* first sort by album */
	ret = compare_string_tag_item(a->tag, b->tag, TAG_ALBUM);
	if (ret != 0)
		return ret;

	/* then sort by disc */
	ret = compare_tag_item(a->tag, b->tag, TAG_DISC);
	if (ret != 0)
		return ret;

	/* then by track number */
	ret = compare_tag_item(a->tag, b->tag, TAG_TRACK);
	if (ret != 0)
		return ret;

	/* still no difference?  compare file name */
	return g_utf8_collate(a->uri, b->uri);
}

static size_t sv_size(const struct songvec *sv)
{
	return sv->nr * sizeof(struct song *);
}

void songvec_init(void)
{
	g_assert(nr_lock == NULL);
	nr_lock = g_mutex_new();
}

void songvec_deinit(void)
{
	g_assert(nr_lock != NULL);
	g_mutex_free(nr_lock);
	nr_lock = NULL;
}

void songvec_sort(struct songvec *sv)
{
	g_mutex_lock(nr_lock);
	qsort(sv->base, sv->nr, sizeof(struct song *), songvec_cmp);
	g_mutex_unlock(nr_lock);
}

struct song *
songvec_find(const struct songvec *sv, const char *uri)
{
	int i;
	struct song *ret = NULL;

	g_mutex_lock(nr_lock);
	for (i = sv->nr; --i >= 0; ) {
		if (strcmp(sv->base[i]->uri, uri))
			continue;
		ret = sv->base[i];
		break;
	}
	g_mutex_unlock(nr_lock);
	return ret;
}

int
songvec_delete(struct songvec *sv, const struct song *del)
{
	size_t i;

	g_mutex_lock(nr_lock);
	for (i = 0; i < sv->nr; ++i) {
		if (sv->base[i] != del)
			continue;
		/* we _don't_ call song_free() here */
		if (!--sv->nr) {
			g_free(sv->base);
			sv->base = NULL;
		} else {
			memmove(&sv->base[i], &sv->base[i + 1],
				(sv->nr - i) * sizeof(struct song *));
			sv->base = g_realloc(sv->base, sv_size(sv));
		}
		g_mutex_unlock(nr_lock);
		return i;
	}
	g_mutex_unlock(nr_lock);

	return -1; /* not found */
}

void
songvec_add(struct songvec *sv, struct song *add)
{
	g_mutex_lock(nr_lock);
	++sv->nr;
	sv->base = g_realloc(sv->base, sv_size(sv));
	sv->base[sv->nr - 1] = add;
	g_mutex_unlock(nr_lock);
}

void songvec_destroy(struct songvec *sv)
{
	g_mutex_lock(nr_lock);
	sv->nr = 0;
	g_mutex_unlock(nr_lock);

	g_free(sv->base);
	sv->base = NULL;
}

int
songvec_for_each(const struct songvec *sv,
		 int (*fn)(struct song *, void *), void *arg)
{
	size_t i;
	size_t prev_nr;

	g_mutex_lock(nr_lock);
	for (i = 0; i < sv->nr; ) {
		struct song *song = sv->base[i];

		assert(song);
		assert(*song->uri);

		prev_nr = sv->nr;
		g_mutex_unlock(nr_lock); /* fn() may block */
		if (fn(song, arg) < 0)
			return -1;
		g_mutex_lock(nr_lock); /* sv->nr may change in fn() */
		if (prev_nr == sv->nr)
			++i;
	}
	g_mutex_unlock(nr_lock);

	return 0;
}
