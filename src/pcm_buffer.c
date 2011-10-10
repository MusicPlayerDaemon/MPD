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

#include "pcm_buffer.h"

void *
pcm_buffer_get(struct pcm_buffer *buffer, size_t size)
{
	if (buffer->size < size) {
		/* free the old buffer */
		g_free(buffer->buffer);

		/* allocate a new buffer; align at 8 kB boundaries */
		buffer->size = ((size - 1) | 0x1fff) + 1;
		buffer->buffer = g_malloc(buffer->size);
	}

	return buffer->buffer;
}
