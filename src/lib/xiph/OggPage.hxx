/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_OGG_PAGE_HXX
#define MPD_OGG_PAGE_HXX

#include <ogg/ogg.h>

#include <cassert>
#include <cstdint>

#include <string.h>

static size_t
ReadPage(const ogg_page &page, void *_buffer, size_t size) noexcept
{
	assert(page.header_len > 0 || page.body_len > 0);

	size_t header_len = (size_t)page.header_len;
	size_t body_len = (size_t)page.body_len;
	assert(header_len <= size);

	if (header_len + body_len > size)
		/* TODO: better overflow handling */
		body_len = size - header_len;

	uint8_t *buffer = (uint8_t *)_buffer;
	memcpy(buffer, page.header, header_len);
	memcpy(buffer + header_len, page.body, body_len);

	return header_len + body_len;
}

#endif
