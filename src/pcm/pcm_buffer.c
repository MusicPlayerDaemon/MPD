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
#include "pcm_buffer.h"
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
pcm_buffer_get(struct pcm_buffer *buffer, size_t size)
{
	assert(buffer != NULL);

	if (size == 0)
		/* never return NULL, because NULL would be assumed to
		   be an error condition */
		size = 1;

	if (buffer->size < size) {
		/* free the old buffer */
		g_free(buffer->buffer);

		buffer->size = align_8k(size);
		buffer->buffer = g_malloc(buffer->size);
	} else {
		/* discard old buffer contents */
		poison_undefined(buffer->buffer, buffer->size);
	}

	assert(buffer->size >= size);

	return buffer->buffer;
}
