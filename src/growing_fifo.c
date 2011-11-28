/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#include "growing_fifo.h"
#include "fifo_buffer.h"

#include <assert.h>
#include <string.h>

/**
 * Align buffer sizes at 8 kB boundaries.  Must be a power of two.
 */
static const size_t GROWING_FIFO_ALIGN = 8192;

/**
 * Align the specified size to the next #GROWING_FIFO_ALIGN boundary.
 */
static size_t
align(size_t size)
{
	return ((size - 1) | (GROWING_FIFO_ALIGN - 1)) + 1;
}

struct fifo_buffer *
growing_fifo_new(void)
{
	return fifo_buffer_new(GROWING_FIFO_ALIGN);
}

void *
growing_fifo_write(struct fifo_buffer **buffer_p, size_t length)
{
	assert(buffer_p != NULL);

	struct fifo_buffer *buffer = *buffer_p;
	assert(buffer != NULL);

	size_t max_length;
	void *p = fifo_buffer_write(buffer, &max_length);
	if (p != NULL && max_length >= length)
		return p;

	/* grow */
	size_t new_size = fifo_buffer_available(buffer) + length;
	assert(new_size > fifo_buffer_capacity(buffer));
	*buffer_p = buffer = fifo_buffer_realloc(buffer, align(new_size));

	/* try again */
	p = fifo_buffer_write(buffer, &max_length);
	assert(p != NULL);
	assert(max_length >= length);

	return p;
}

void
growing_fifo_append(struct fifo_buffer **buffer_p,
		    const void *data, size_t length)
{
	void *p = growing_fifo_write(buffer_p, length);
	memcpy(p, data, length);
	fifo_buffer_append(*buffer_p, length);
}
