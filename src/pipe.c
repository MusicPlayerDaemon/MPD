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
#include "utils.h"

#include <assert.h>
#include <string.h>

struct music_pipe ob;

void
music_pipe_init(unsigned int size, struct notify *notify)
{
	assert(size > 0);

	ob.chunks = xmalloc(size * sizeof(*ob.chunks));
	ob.size = size;
	ob.begin = 0;
	ob.end = 0;
	ob.lazy = false;
	ob.notify = notify;
	ob.chunks[0].chunkSize = 0;
}

void music_pipe_free(void)
{
	assert(ob.chunks != NULL);
	free(ob.chunks);
}

void music_pipe_clear(void)
{
	ob.end = ob.begin;
	ob.chunks[ob.end].chunkSize = 0;
}

/** return the index of the chunk after i */
static inline unsigned successor(unsigned i)
{
	assert(i <= ob.size);

	++i;
	return i == ob.size ? 0 : i;
}

/**
 * Mark the tail chunk as "full" and wake up the player if is waiting
 * for the decoder.
 */
static void output_buffer_expand(unsigned i)
{
	int was_empty = ob.notify != NULL && (!ob.lazy || music_pipe_is_empty());

	assert(i == (ob.end + 1) % ob.size);
	assert(i != ob.end);

	ob.end = i;
	ob.chunks[i].chunkSize = 0;
	if (was_empty)
		/* if the buffer was empty, the player thread might be
		   waiting for us; wake it up now that another decoded
		   buffer has become available. */
		notify_signal(ob.notify);
}

void music_pipe_flush(void)
{
	struct music_chunk *chunk = music_pipe_get_chunk(ob.end);

	if (chunk->chunkSize > 0) {
		unsigned int next = successor(ob.end);
		if (next == ob.begin)
			/* all buffers are full; we have to wait for
			   the player to free one, so don't flush
			   right now */
			return;

		output_buffer_expand(next);
	}
}

void music_pipe_set_lazy(bool lazy)
{
	ob.lazy = lazy;
}

void music_pipe_shift(void)
{
	assert(ob.begin != ob.end);
	assert(ob.begin < ob.size);

	ob.begin = successor(ob.begin);
}

unsigned int music_pipe_relative(const unsigned i)
{
	if (i >= ob.begin)
		return i - ob.begin;
	else
		return i + ob.size - ob.begin;
}

unsigned music_pipe_available(void)
{
	return music_pipe_relative(ob.end);
}

int music_pipe_absolute(const unsigned relative)
{
	unsigned i, max;

	max = ob.end;
	if (max < ob.begin)
		max += ob.size;
	i = (unsigned)ob.begin + relative;
	if (i >= max)
		return -1;

	if (i >= ob.size)
		i -= ob.size;

	return (int)i;
}

struct music_chunk *
music_pipe_get_chunk(const unsigned i)
{
	assert(i < ob.size);

	return &ob.chunks[i];
}

/**
 * Return the tail chunk which has room for additional data.
 *
 * @return the chunk which has room for more data; NULL if there is no
 * room.
 */
static struct music_chunk *
tail_chunk(float data_time, uint16_t bitRate)
{
	const size_t frame_size = audio_format_frame_size(&ob.audioFormat);
	unsigned int next;
	struct music_chunk *chunk;

	chunk = music_pipe_get_chunk(ob.end);
	assert(chunk->chunkSize <= sizeof(chunk->data));
	if (chunk->chunkSize + frame_size > sizeof(chunk->data)) {
		/* this chunk is full; allocate a new chunk */
		next = successor(ob.end);
		if (ob.begin == next)
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
			 float data_time, uint16_t bitRate)
{
	const unsigned char *data = data0;
	const size_t frame_size = audio_format_frame_size(&ob.audioFormat);
	size_t ret = 0, dataToSend;
	struct music_chunk *chunk = NULL;

	/* no partial frames allowed */
	assert((datalen % frame_size) == 0);

	while (datalen) {
		chunk = tail_chunk(data_time, bitRate);
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
		ob.begin = i;
}
