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
#include "page.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

/**
 * Allocates a new #page object, without filling the data element.
 */
static struct page *
page_new(size_t size)
{
	struct page *page = g_malloc(sizeof(*page) + size -
				     sizeof(page->data));

	assert(size > 0);

	page->ref = 1;
	page->size = size;
	return page;
}

struct page *
page_new_copy(const void *data, size_t size)
{
	struct page *page = page_new(size);

	assert(data != NULL);

	memcpy(page->data, data, size);
	return page;
}

struct page *
page_new_concat(const struct page *a, const struct page *b)
{
	struct page *page = page_new(a->size + b->size);

	memcpy(page->data, a->data, a->size);
	memcpy(page->data + a->size, b->data, b->size);

	return page;
}

void
page_ref(struct page *page)
{
	g_atomic_int_inc(&page->ref);
}

static void
page_free(struct page *page)
{
	assert(page->ref == 0);

	g_free(page);
}

bool
page_unref(struct page *page)
{
	bool unused = g_atomic_int_dec_and_test(&page->ref);

	if (unused)
		page_free(page);

	return unused;
}
