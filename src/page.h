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

/** \file
 *
 * This is a library which manages reference counted buffers.
 */

#ifndef MPD_PAGE_H
#define MPD_PAGE_H

#include <stddef.h>
#include <stdbool.h>

/**
 * A dynamically allocated buffer which keeps track of its reference
 * count.  This is useful for passing buffers around, when several
 * instances hold references to one buffer.
 */
struct page {
	/**
	 * The number of references to this buffer.  This library uses
	 * atomic functions to access it, i.e. no locks are required.
	 * As soon as this attribute reaches zero, the buffer is
	 * freed.
	 */
	int ref;

	/**
	 * The size of this buffer in bytes.
	 */
	size_t size;

	/**
	 * Dynamic array containing the buffer data.
	 */
	unsigned char data[sizeof(long)];
};

/**
 * Creates a new #page object, and copies data from the specified
 * buffer.  It is initialized with a reference count of 1.
 *
 * @param data the source buffer
 * @param size the size of the source buffer
 * @return the new #page object
 */
struct page *
page_new_copy(const void *data, size_t size);

/**
 * Concatenates two pages to a new page.
 *
 * @param a the first page
 * @param b the second page, which is appended
 */
struct page *
page_new_concat(const struct page *a, const struct page *b);

/**
 * Increases the reference counter.
 *
 * @param page the #page object
 */
void
page_ref(struct page *page);

/**
 * Decreases the reference counter.  If it reaches zero, the #page is
 * freed.
 *
 * @param page the #page object
 * @return true if the #page has been freed
 */
bool
page_unref(struct page *page);

#endif
