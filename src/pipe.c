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
#include "pipe.h"
#include "buffer.h"
#include "chunk.h"

#include <glib.h>

#include <assert.h>

struct music_pipe {
	/** the first chunk */
	struct music_chunk *head;

	/** a pointer to the tail of the chunk */
	struct music_chunk **tail_r;

	/** the current number of chunks */
	unsigned size;

	/** a mutex which protects #head and #tail_r */
	GMutex *mutex;

#ifndef NDEBUG
	struct audio_format audio_format;
#endif
};

struct music_pipe *
music_pipe_new(void)
{
	struct music_pipe *mp = g_new(struct music_pipe, 1);

	mp->head = NULL;
	mp->tail_r = &mp->head;
	mp->size = 0;
	mp->mutex = g_mutex_new();

#ifndef NDEBUG
	audio_format_clear(&mp->audio_format);
#endif

	return mp;
}

void
music_pipe_free(struct music_pipe *mp)
{
	assert(mp->head == NULL);
	assert(mp->tail_r == &mp->head);

	g_mutex_free(mp->mutex);
	g_free(mp);
}

#ifndef NDEBUG

bool
music_pipe_check_format(const struct music_pipe *pipe,
			const struct audio_format *audio_format)
{
	assert(pipe != NULL);
	assert(audio_format != NULL);

	return !audio_format_defined(&pipe->audio_format) ||
		audio_format_equals(&pipe->audio_format, audio_format);
}

bool
music_pipe_contains(const struct music_pipe *mp,
		    const struct music_chunk *chunk)
{
	g_mutex_lock(mp->mutex);

	for (const struct music_chunk *i = mp->head;
	     i != NULL; i = i->next) {
		if (i == chunk) {
			g_mutex_unlock(mp->mutex);
			return true;
		}
	}

	g_mutex_unlock(mp->mutex);

	return false;
}

#endif

const struct music_chunk *
music_pipe_peek(const struct music_pipe *mp)
{
	return mp->head;
}

struct music_chunk *
music_pipe_shift(struct music_pipe *mp)
{
	struct music_chunk *chunk;

	g_mutex_lock(mp->mutex);

	chunk = mp->head;
	if (chunk != NULL) {
		assert(!music_chunk_is_empty(chunk));

		mp->head = chunk->next;
		--mp->size;

		if (mp->head == NULL) {
			assert(mp->size == 0);
			assert(mp->tail_r == &chunk->next);

			mp->tail_r = &mp->head;
		} else {
			assert(mp->size > 0);
			assert(mp->tail_r != &chunk->next);
		}

#ifndef NDEBUG
		/* poison the "next" reference */
		chunk->next = (void*)0x01010101;

		if (mp->size == 0)
			audio_format_clear(&mp->audio_format);
#endif
	}

	g_mutex_unlock(mp->mutex);

	return chunk;
}

void
music_pipe_clear(struct music_pipe *mp, struct music_buffer *buffer)
{
	struct music_chunk *chunk;

	while ((chunk = music_pipe_shift(mp)) != NULL)
		music_buffer_return(buffer, chunk);
}

void
music_pipe_push(struct music_pipe *mp, struct music_chunk *chunk)
{
	assert(!music_chunk_is_empty(chunk));
	assert(chunk->length == 0 || audio_format_valid(&chunk->audio_format));

	g_mutex_lock(mp->mutex);

	assert(mp->size > 0 || !audio_format_defined(&mp->audio_format));
	assert(!audio_format_defined(&mp->audio_format) ||
	       music_chunk_check_format(chunk, &mp->audio_format));

#ifndef NDEBUG
	if (!audio_format_defined(&mp->audio_format) && chunk->length > 0)
		mp->audio_format = chunk->audio_format;
#endif

	chunk->next = NULL;
	*mp->tail_r = chunk;
	mp->tail_r = &chunk->next;

	++mp->size;

	g_mutex_unlock(mp->mutex);
}

unsigned
music_pipe_size(const struct music_pipe *mp)
{
	return mp->size;
}
