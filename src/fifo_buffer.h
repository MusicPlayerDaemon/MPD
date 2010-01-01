/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
 * This is a general purpose FIFO buffer library.  You may append data
 * at the end, while another instance reads data from the beginning.
 * It is optimized for zero-copy usage: you get pointers to the real
 * buffer, where you may operate on.
 *
 * This library is not thread safe.
 */

#ifndef MPD_FIFO_BUFFER_H
#define MPD_FIFO_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

struct fifo_buffer;

/**
 * Creates a new #fifo_buffer object.  Free this object with
 * fifo_buffer_free().
 *
 * @param size the size of the buffer in bytes
 * @return the new #fifo_buffer object
 */
struct fifo_buffer *
fifo_buffer_new(size_t size);

/**
 * Frees the resources consumed by this #fifo_buffer object.
 */
void
fifo_buffer_free(struct fifo_buffer *buffer);

/**
 * Clears all data currently in this #fifo_buffer object.  This does
 * not overwrite the actuall buffer; it just resets the internal
 * pointers.
 */
void
fifo_buffer_clear(struct fifo_buffer *buffer);

/**
 * Reads from the beginning of the buffer.  To remove consumed data
 * from the buffer, call fifo_buffer_consume().
 *
 * @param buffer the #fifo_buffer object
 * @param length_r the maximum amount to read is returned here
 * @return a pointer to the beginning of the buffer, or NULL if the
 * buffer is empty
 */
const void *
fifo_buffer_read(const struct fifo_buffer *buffer, size_t *length_r);

/**
 * Marks data at the beginning of the buffer as "consumed".
 *
 * @param buffer the #fifo_buffer object
 * @param length the number of bytes which were consumed
 */
void
fifo_buffer_consume(struct fifo_buffer *buffer, size_t length);

/**
 * Prepares writing to the buffer.  This returns a buffer which you
 * can write to.  To commit the write operation, call
 * fifo_buffer_append().
 *
 * @param buffer the #fifo_buffer object
 * @param max_length_r the maximum amount to write is returned here
 * @return a pointer to the end of the buffer, or NULL if the buffer
 * is already full
 */
void *
fifo_buffer_write(struct fifo_buffer *buffer, size_t *max_length_r);

/**
 * Commits the write operation initiated by fifo_buffer_write().
 *
 * @param buffer the #fifo_buffer object
 * @param length the number of bytes which were written
 */
void
fifo_buffer_append(struct fifo_buffer *buffer, size_t length);

/**
 * Checks if the buffer is empty.
 */
bool
fifo_buffer_is_empty(struct fifo_buffer *buffer);

/**
 * Checks if the buffer is full.
 */
bool
fifo_buffer_is_full(struct fifo_buffer *buffer);

#endif
