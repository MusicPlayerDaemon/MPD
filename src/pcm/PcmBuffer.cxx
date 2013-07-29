/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PcmBuffer.hxx"
#include "poison.h"

/**
 * Align the specified size to the next 8k boundary.
 */
G_GNUC_CONST
static size_t
align_8k(size_t size)
{
	return ((size - 1) | 0x1fff) + 1;
}

void *
PcmBuffer::Get(size_t new_size)
{
	if (new_size == 0)
		/* never return NULL, because NULL would be assumed to
		   be an error condition */
		new_size = 1;

	if (size < new_size) {
		/* free the old buffer */
		g_free(buffer);

		size = align_8k(new_size);
		buffer = g_malloc(size);
	} else {
		/* discard old buffer contents */
		poison_undefined(buffer, size);
	}

	assert(size >= new_size);

	return buffer;
}
