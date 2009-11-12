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
#include "buffer.h"
#include "chunk.h"
#include "poison.h"

#include <glib.h>

#include <assert.h>

struct music_buffer {
	struct music_chunk *chunks;
	unsigned num_chunks;

	struct music_chunk *available;

	/** a mutex which protects #available */
	GMutex *mutex;

#ifndef NDEBUG
	unsigned num_allocated;
#endif
};

struct music_buffer *
music_buffer_new(unsigned num_chunks)
{
	struct music_buffer *buffer;
	struct music_chunk *chunk;

	assert(num_chunks > 0);

	buffer = g_new(struct music_buffer, 1);

	buffer->chunks = g_new(struct music_chunk, num_chunks);
	buffer->num_chunks = num_chunks;

	chunk = buffer->available = buffer->chunks;
	poison_undefined(chunk, sizeof(*chunk));

	for (unsigned i = 1; i < num_chunks; ++i) {
		chunk->next = &buffer->chunks[i];
		chunk = chunk->next;
		poison_undefined(chunk, sizeof(*chunk));
	}

	chunk->next = NULL;

	buffer->mutex = g_mutex_new();

#ifndef NDEBUG
	buffer->num_allocated = 0;
#endif

	return buffer;
}

void
music_buffer_free(struct music_buffer *buffer)
{
	assert(buffer->chunks != NULL);
	assert(buffer->num_chunks > 0);
	assert(buffer->num_allocated == 0);

	g_mutex_free(buffer->mutex);
	g_free(buffer->chunks);
	g_free(buffer);
}

unsigned
music_buffer_size(const struct music_buffer *buffer)
{
	return buffer->num_chunks;
}

struct music_chunk *
music_buffer_allocate(struct music_buffer *buffer)
{
	struct music_chunk *chunk;

	g_mutex_lock(buffer->mutex);

	chunk = buffer->available;
	if (chunk != NULL) {
		buffer->available = chunk->next;
		music_chunk_init(chunk);

#ifndef NDEBUG
		++buffer->num_allocated;
#endif
	}

	g_mutex_unlock(buffer->mutex);
	return chunk;
}

void
music_buffer_return(struct music_buffer *buffer, struct music_chunk *chunk)
{
	assert(buffer != NULL);
	assert(chunk != NULL);

	g_mutex_lock(buffer->mutex);

	music_chunk_free(chunk);
	poison_undefined(chunk, sizeof(*chunk));

	chunk->next = buffer->available;
	buffer->available = chunk;

#ifndef NDEBUG
	--buffer->num_allocated;
#endif

	g_mutex_unlock(buffer->mutex);
}
