/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "fifo_buffer.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

struct fifo_buffer {
	size_t size, start, end;
	unsigned char buffer[sizeof(size_t)];
};

struct fifo_buffer *
fifo_buffer_new(size_t size)
{
	struct fifo_buffer *buffer;

	assert(size > 0);

	buffer = (struct fifo_buffer *)g_malloc(sizeof(*buffer) -
						sizeof(buffer->buffer) + size);

	buffer->size = size;
	buffer->start = 0;
	buffer->end = 0;

	return buffer;
}

void
fifo_buffer_free(struct fifo_buffer *buffer)
{
	assert(buffer != NULL);

	g_free(buffer);
}

void
fifo_buffer_clear(struct fifo_buffer *buffer)
{
	assert(buffer != NULL);

	buffer->start = 0;
	buffer->end = 0;
}

const void *
fifo_buffer_read(const struct fifo_buffer *buffer, size_t *length_r)
{
	assert(buffer != NULL);
	assert(buffer->end >= buffer->start);
	assert(length_r != NULL);

	if (buffer->start == buffer->end)
		/* the buffer is empty */
		return NULL;

	*length_r = buffer->end - buffer->start;
	return buffer->buffer + buffer->start;
}

void
fifo_buffer_consume(struct fifo_buffer *buffer, size_t length)
{
	assert(buffer != NULL);
	assert(buffer->end >= buffer->start);
	assert(buffer->start + length <= buffer->end);

	buffer->start += length;
}

/**
 * Move data to the beginning of the buffer, to make room at the end.
 */
static void
fifo_buffer_move(struct fifo_buffer *buffer)
{
	if (buffer->start == 0)
		return;

	if (buffer->end > buffer->start)
		memmove(buffer->buffer,
			buffer->buffer + buffer->start,
			buffer->end - buffer->start);

	buffer->end -= buffer->start;
	buffer->start = 0;
}

void *
fifo_buffer_write(struct fifo_buffer *buffer, size_t *max_length_r)
{
	assert(buffer != NULL);
	assert(buffer->end <= buffer->size);
	assert(max_length_r != NULL);

	if (buffer->end == buffer->size) {
		fifo_buffer_move(buffer);
		if (buffer->end == buffer->size)
			return NULL;
	} else if (buffer->start > 0 && buffer->start == buffer->end) {
		buffer->start = 0;
		buffer->end = 0;
	}

	*max_length_r = buffer->size - buffer->end;
	return buffer->buffer + buffer->end;
}

void
fifo_buffer_append(struct fifo_buffer *buffer, size_t length)
{
	assert(buffer != NULL);
	assert(buffer->end >= buffer->start);
	assert(buffer->end + length <= buffer->size);

	buffer->end += length;
}

bool
fifo_buffer_is_empty(struct fifo_buffer *buffer)
{
	return buffer->start == buffer->end;
}

bool
fifo_buffer_is_full(struct fifo_buffer *buffer)
{
	return buffer->start == 0 && buffer->end == buffer->size;
}
