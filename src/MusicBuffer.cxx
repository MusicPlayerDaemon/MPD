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
#include "MusicBuffer.hxx"
#include "MusicChunk.hxx"
#include "util/SliceBuffer.hxx"

#include <glib.h>

#include <assert.h>

struct music_buffer : public SliceBuffer<music_chunk>  {
	/** a mutex which protects #available */
	GMutex *mutex;

	music_buffer(unsigned num_chunks)
		:SliceBuffer(num_chunks),
		 mutex(g_mutex_new()) {}

	~music_buffer() {
		g_mutex_free(mutex);
	}
};

struct music_buffer *
music_buffer_new(unsigned num_chunks)
{
	return new music_buffer(num_chunks);
}

void
music_buffer_free(struct music_buffer *buffer)
{
	delete buffer;
}

unsigned
music_buffer_size(const struct music_buffer *buffer)
{
	return buffer->GetCapacity();
}

struct music_chunk *
music_buffer_allocate(struct music_buffer *buffer)
{
	g_mutex_lock(buffer->mutex);
	struct music_chunk *chunk = buffer->Allocate();
	g_mutex_unlock(buffer->mutex);
	return chunk;
}

void
music_buffer_return(struct music_buffer *buffer, struct music_chunk *chunk)
{
	assert(buffer != NULL);
	assert(chunk != NULL);

	g_mutex_lock(buffer->mutex);

	if (chunk->other != nullptr) {
		assert(chunk->other->other == nullptr);
		buffer->Free(chunk->other);
	}

	buffer->Free(chunk);

	g_mutex_unlock(buffer->mutex);
}
