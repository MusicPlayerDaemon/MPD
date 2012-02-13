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

#ifndef PCM_BUFFER_H
#define PCM_BUFFER_H

#include "check.h"

#include <glib.h>

#include <assert.h>

/**
 * Manager for a temporary buffer which grows as needed.  We could
 * allocate a new buffer every time pcm_convert() is called, but that
 * would put too much stress on the allocator.
 */
struct pcm_buffer {
	void *buffer;

	size_t size;
};

/**
 * Initialize the buffer, but don't allocate anything yet.
 */
static inline void
pcm_buffer_init(struct pcm_buffer *buffer)
{
	assert(buffer != NULL);

	buffer->buffer = NULL;
	buffer->size = 0;
}

/**
 * Free resources.  This function may be called more than once.
 */
static inline void
pcm_buffer_deinit(struct pcm_buffer *buffer)
{
	assert(buffer != NULL);

	g_free(buffer->buffer);

	buffer->buffer = NULL;
}

/**
 * Get the buffer, and guarantee a minimum size.  This buffer becomes
 * invalid with the next pcm_buffer_get() call.
 *
 * This function will never return NULL, even if size is zero, because
 * the PCM library uses the NULL return value to signal "error".  An
 * empty destination buffer is not always an error.
 */
G_GNUC_MALLOC
void *
pcm_buffer_get(struct pcm_buffer *buffer, size_t size);

#endif
