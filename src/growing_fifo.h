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

/** \file
 *
 * Helper functions for our FIFO buffer library (fifo_buffer.h) that
 * allows growing the buffer on demand.
 *
 * This library is not thread safe.
 */

#ifndef MPD_GROWING_FIFO_H
#define MPD_GROWING_FIFO_H

#include <stddef.h>

struct fifo_buffer;

/**
 * Allocate a new #fifo_buffer with the default size.
 */
struct fifo_buffer *
growing_fifo_new(void);

/**
 * Prepares writing to the buffer, see fifo_buffer_write() for
 * details.  The difference is that this function will automatically
 * grow the buffer if it is too small.
 *
 * The caller is responsible for limiting the capacity of the buffer.
 *
 * @param length the number of bytes that will be written
 * @return a pointer to the end of the buffer (will not be NULL)
 */
void *
growing_fifo_write(struct fifo_buffer **buffer_p, size_t length);

/**
 * A helper function that combines growing_fifo_write(), memcpy(),
 * fifo_buffer_append().
 */
void
growing_fifo_append(struct fifo_buffer **buffer_p,
		    const void *data, size_t length);

#endif
