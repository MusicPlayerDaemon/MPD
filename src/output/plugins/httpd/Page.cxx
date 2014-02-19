/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Page.hxx"
#include "util/Alloc.hxx"

#include <new>

#include <assert.h>
#include <string.h>
#include <stdlib.h>

Page *
Page::Create(size_t size)
{
	void *p = xalloc(sizeof(Page) + size -
			 sizeof(Page::data));
	return ::new(p) Page(size);
}

Page *
Page::Copy(const void *data, size_t size)
{
	assert(data != nullptr);

	Page *page = Create(size);
	memcpy(page->data, data, size);
	return page;
}

Page *
Page::Concat(const Page &a, const Page &b)
{
	Page *page = Create(a.size + b.size);

	memcpy(page->data, a.data, a.size);
	memcpy(page->data + a.size, b.data, b.size);

	return page;
}

bool
Page::Unref()
{
	bool unused = ref.Decrement();

	if (unused) {
		this->Page::~Page();
		free(this);
	}

	return unused;
}
