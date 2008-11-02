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
#include "notify.h"
#include "audio_format.h"

#include <glib.h>
#include <assert.h>
#include <string.h>

struct music_pipe music_pipe;

void
music_pipe_init(unsigned int size, struct notify *notify)
{
	assert(size > 0);

	music_pipe.chunks = g_new(struct music_chunk, size);
	music_pipe.size = size;
	music_pipe.begin = 0;
	music_pipe.end = 0;
	music_pipe.lazy = false;
	music_pipe.notify = notify;
	music_pipe.chunks[0].chunkSize = 0;
}

void music_pipe_free(void)
{
	assert(music_pipe.chunks != NULL);
	g_free(music_pipe.chunks);
}

void music_pipe_clear(void)
{
	music_pipe.end = music_pipe.begin;
	music_pipe.chunks[music_pipe.end].chunkSize = 0;
}

/** return the index of the chunk after i */
static inline unsigned successor(unsigned i)
{
	assert(i <= music_pipe.size);

	++i;
	return i == music_pipe.size ? 0 : i;
}

/**
 * Mark the tail chunk as "full" and wake up the player if is waiting
 * for the decoder.
 */
static void output_buffer_expand(unsigned i)
{
	int was_empty = music_pipe.notify != NULL && (!music_pipe.lazy || music_pipe_is_empty());

	assert(i == (music_pipe.end + 1) % music_pipe.size);
	assert(i != music_pipe.end);

	music_pipe.end = i;
	music_pipe.chunks[i].chunkSize = 0;
	if (was_empty)
		/* if the buffer was empty, the player thread might be
		   waiting for us; wake it up now that another decoded
		   buffer has become available. */
		notify_signal(music_pipe.notify);
}

void music_pipe_flush(void)
{
	struct music_chunk *chunk = music_pipe_get_chunk(music_pipe.end);

	if (chunk->chunkSize > 0) {
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
	assert(music_pipe.begin < music_pipe.size);

	music_pipe.begin = successor(music_pipe.begin);
}

unsigned int music_pipe_relative(const unsigned i)
{
	if (i >= music_pipe.begin)
		return i - music_pipe.begin;
	else
		return i + music_pipe.size - music_pipe.begin;
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
		max += music_pipe.size;
	i = (unsigned)music_pipe.begin + relative;
	if (i >= max)
		return -1;

	if (i >= music_pipe.size)
		i -= music_pipe.size;

	return (int)i;
}

struct music_chunk *
music_pipe_get_chunk(const unsigned i)
{
	assert(i < music_pipe.size);

	return &music_pipe.chunks[i];
}

/**
 * Return the tail chunk which has room for additional data.
 *
 * @return the chunk which has room for more data; NULL if there is no
 * room.
 */
static struct music_chunk *
tail_chunk(float data_time, uint16_t bitRate, size_t frame_size)
{
	unsigned int next;
	struct music_chunk *chunk;

	chunk = music_pipe_get_chunk(music_pipe.end);
	assert(chunk->chunkSize <= sizeof(chunk->data));
	if (chunk->chunkSize + frame_size > sizeof(chunk->data)) {
		/* this chunk is full; allocate a new chunk */
		next = successor(music_pipe.end);
		if (music_pipe.begin == next)
			/* no chunks available */
			return NULL;

		output_buffer_expand(next);
		chunk = music_pipe_get_chunk(next);
		assert(chunk->chunkSize == 0);
	}

	if (chunk->chunkSize == 0) {
		/* if the chunk is empty, nobody has set bitRate and
		   times yet */

		chunk->bitRate = bitRate;
		chunk->times = data_time;
	}

	return chunk;
}

size_t music_pipe_append(const void *data0, size_t datalen,
			 const struct audio_format *audio_format,
			 float data_time, uint16_t bitRate)
{
	const unsigned char *data = data0;
	const size_t frame_size = audio_format_frame_size(audio_format);
	size_t ret = 0, dataToSend;
	struct music_chunk *chunk = NULL;

	/* no partial frames allowed */
	assert((datalen % frame_size) == 0);

	while (datalen) {
		chunk = tail_chunk(data_time, bitRate, frame_size);
		if (chunk == NULL)
			return ret;

		dataToSend = sizeof(chunk->data) - chunk->chunkSize;
		if (dataToSend > datalen)
			dataToSend = datalen;

		/* don't split frames */
		dataToSend /= frame_size;
		dataToSend *= frame_size;

		memcpy(chunk->data + chunk->chunkSize, data, dataToSend);
		chunk->chunkSize += dataToSend;
		datalen -= dataToSend;
		data += dataToSend;
		ret += dataToSend;
	}

	if (chunk != NULL && chunk->chunkSize == sizeof(chunk->data))
		music_pipe_flush();

	return ret;
}

void music_pipe_skip(unsigned num)
{
	int i = music_pipe_absolute(num);
	if (i >= 0)
		music_pipe.begin = i;
}
