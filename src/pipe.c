/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pipe.h"
#include "chunk.h"
#include "notify.h"
#include "audio_format.h"
#include "tag.h"

#include <glib.h>
#include <assert.h>
#include <string.h>

struct music_pipe music_pipe;

void
music_pipe_init(unsigned int size, struct notify *notify)
{
	assert(size > 0);

	music_pipe.chunks = g_new(struct music_chunk, size);
	music_pipe.num_chunks = size;
	music_pipe.begin = 0;
	music_pipe.end = 0;
	music_pipe.lazy = false;
	music_pipe.notify = notify;
	music_chunk_init(&music_pipe.chunks[0]);
}

void music_pipe_free(void)
{
	assert(music_pipe.chunks != NULL);

	music_pipe_clear();

	g_free(music_pipe.chunks);
}

/** return the index of the chunk after i */
static inline unsigned successor(unsigned i)
{
	assert(i < music_pipe.num_chunks);

	++i;
	return i == music_pipe.num_chunks ? 0 : i;
}

void music_pipe_clear(void)
{
	unsigned i;

	for (i = music_pipe.begin; i != music_pipe.end; i = successor(i))
		music_chunk_free(&music_pipe.chunks[i]);

	music_chunk_free(&music_pipe.chunks[music_pipe.end]);

	music_pipe.end = music_pipe.begin;
	music_chunk_init(&music_pipe.chunks[music_pipe.end]);
}

/**
 * Mark the tail chunk as "full" and wake up the player if is waiting
 * for the decoder.
 */
static void output_buffer_expand(unsigned i)
{
	int was_empty = music_pipe.notify != NULL && (!music_pipe.lazy || music_pipe_is_empty());

	assert(i == (music_pipe.end + 1) % music_pipe.num_chunks);
	assert(i != music_pipe.end);

	music_pipe.end = i;
	music_chunk_init(&music_pipe.chunks[i]);

	if (was_empty)
		/* if the buffer was empty, the player thread might be
		   waiting for us; wake it up now that another decoded
		   buffer has become available. */
		notify_signal(music_pipe.notify);
}

void music_pipe_flush(void)
{
	struct music_chunk *chunk = music_pipe_get_chunk(music_pipe.end);

	if (chunk->length > 0) {
		unsigned int next = successor(music_pipe.end);
		if (next == music_pipe.begin)
			/* all buffers are full; we have to wait for
			   the player to free one, so don't flush
			   right now */
			return;

		output_buffer_expand(next);
	}
}

void music_pipe_set_lazy(bool lazy)
{
	music_pipe.lazy = lazy;
}

void music_pipe_shift(void)
{
	assert(music_pipe.begin != music_pipe.end);
	assert(music_pipe.begin < music_pipe.num_chunks);

	music_chunk_free(&music_pipe.chunks[music_pipe.begin]);

	music_pipe.begin = successor(music_pipe.begin);
}

unsigned int music_pipe_relative(const unsigned i)
{
	if (i >= music_pipe.begin)
		return i - music_pipe.begin;
	else
		return i + music_pipe.num_chunks - music_pipe.begin;
}

unsigned music_pipe_available(void)
{
	return music_pipe_relative(music_pipe.end);
}

int music_pipe_absolute(const unsigned relative)
{
	unsigned i, max;

	max = music_pipe.end;
	if (max < music_pipe.begin)
		max += music_pipe.num_chunks;
	i = (unsigned)music_pipe.begin + relative;
	if (i >= max)
		return -1;

	if (i >= music_pipe.num_chunks)
		i -= music_pipe.num_chunks;

	return (int)i;
}

struct music_chunk *
music_pipe_get_chunk(const unsigned i)
{
	assert(i < music_pipe.num_chunks);

	return &music_pipe.chunks[i];
}

/**
 * Return the tail chunk which has room for additional data.
 *
 * @return the chunk which has room for more data; NULL if there is no
 * room.
 */
static struct music_chunk *
tail_chunk(size_t frame_size)
{
	unsigned int next;
	struct music_chunk *chunk;

	chunk = music_pipe_get_chunk(music_pipe.end);
	assert(chunk->length <= sizeof(chunk->data));
	assert((chunk->length % frame_size) == 0);

	if (chunk->length + frame_size > sizeof(chunk->data)) {
		/* this chunk is full; allocate a new chunk */
		next = successor(music_pipe.end);
		if (music_pipe.begin == next)
			/* no chunks available */
			return NULL;

		output_buffer_expand(next);
		chunk = music_pipe_get_chunk(next);
		assert(chunk->length == 0);
	}

	return chunk;
}

void *
music_pipe_write(const struct audio_format *audio_format,
		 float data_time, uint16_t bit_rate,
		 size_t *max_length_r)
{
	const size_t frame_size = audio_format_frame_size(audio_format);
	struct music_chunk *chunk;

	chunk = tail_chunk(frame_size);
	if (chunk == NULL)
		return NULL;

	return music_chunk_write(chunk, audio_format, data_time, bit_rate,
				 max_length_r);
}

void
music_pipe_expand(const struct audio_format *audio_format, size_t length)
{
	const size_t frame_size = audio_format_frame_size(audio_format);
	struct music_chunk *chunk;
	bool full;

	/* no partial frames allowed */
	assert(length % frame_size == 0);

	chunk = tail_chunk(frame_size);
	assert(chunk != NULL);

	full = music_chunk_expand(chunk, audio_format, length);
	if (full)
		music_pipe_flush();
}

bool music_pipe_tag(const struct tag *tag)
{
	struct music_chunk *chunk;

	chunk = music_pipe_get_chunk(music_pipe.end);
	if (chunk->length > 0 || chunk->tag != NULL) {
		/* this chunk is not empty; allocate a new chunk,
		   because chunk.tag refers to the beginning of the
		   chunk data */
		unsigned next = successor(music_pipe.end);
		if (music_pipe.begin == next)
			/* no chunks available */
			return false;

		output_buffer_expand(next);
		chunk = music_pipe_get_chunk(next);
		assert(chunk->length == 0 && chunk->tag == NULL);
	}

	chunk->tag = tag_dup(tag);
	return true;
}

void music_pipe_skip(unsigned num)
{
	int i = music_pipe_absolute(num);
	if (i < 0)
		return;

	while (music_pipe.begin != (unsigned)i)
		music_pipe_shift();
}

void music_pipe_chop(unsigned first)
{
	for (unsigned i = first; i != music_pipe.end; i = successor(i))
		music_chunk_free(&music_pipe.chunks[i]);

	music_chunk_free(&music_pipe.chunks[music_pipe.end]);

	music_pipe.end = first;
	music_chunk_init(&music_pipe.chunks[first]);

}

#ifndef NDEBUG
void music_pipe_check_format(const struct audio_format *current,
			     int next_index, const struct audio_format *next)
{
	const struct audio_format *audio_format = current;

	for (unsigned i = music_pipe.begin; i != music_pipe.end;
	     i = successor(i)) {
		const struct music_chunk *chunk = music_pipe_get_chunk(i);

		if (next_index > 0 && i == (unsigned)next_index)
			audio_format = next;

		assert(chunk->length % audio_format_frame_size(audio_format) == 0);
	}
}
#endif
