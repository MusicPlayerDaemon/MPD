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
#include "decoder_buffer.h"
#include "decoder_api.h"

#include <glib.h>

#include <assert.h>

struct decoder_buffer {
	struct decoder *decoder;
	struct input_stream *is;

	/** the allocated size of the buffer */
	size_t size;

	/** the current length of the buffer */
	size_t length;

	/** number of bytes already consumed at the beginning of the
	    buffer */
	size_t consumed;

	/** the actual buffer (dynamic size) */
	unsigned char data[sizeof(size_t)];
};

struct decoder_buffer *
decoder_buffer_new(struct decoder *decoder, struct input_stream *is,
		   size_t size)
{
	struct decoder_buffer *buffer =
		g_malloc(sizeof(*buffer) - sizeof(buffer->data) + size);

	assert(is != NULL);
	assert(size > 0);

	buffer->decoder = decoder;
	buffer->is = is;
	buffer->size = size;
	buffer->length = 0;
	buffer->consumed = 0;

	return buffer;
}

void
decoder_buffer_free(struct decoder_buffer *buffer)
{
	assert(buffer != NULL);

	g_free(buffer);
}

bool
decoder_buffer_is_empty(const struct decoder_buffer *buffer)
{
	return buffer->consumed == buffer->length;
}

bool
decoder_buffer_is_full(const struct decoder_buffer *buffer)
{
	return buffer->consumed == 0 && buffer->length == buffer->size;
}

static void
decoder_buffer_shift(struct decoder_buffer *buffer)
{
	assert(buffer->consumed > 0);

	buffer->length -= buffer->consumed;
	memmove(buffer->data, buffer->data + buffer->consumed, buffer->length);
	buffer->consumed = 0;
}

bool
decoder_buffer_fill(struct decoder_buffer *buffer)
{
	size_t nbytes;

	if (buffer->consumed > 0)
		decoder_buffer_shift(buffer);

	if (buffer->length >= buffer->size)
		/* buffer is full */
		return false;

	nbytes = decoder_read(buffer->decoder, buffer->is,
			      buffer->data + buffer->length,
			      buffer->size - buffer->length);
	if (nbytes == 0)
		/* end of file, I/O error or decoder command
		   received */
		return false;

	buffer->length += nbytes;
	assert(buffer->length <= buffer->size);

	return true;
}

const void *
decoder_buffer_read(const struct decoder_buffer *buffer, size_t *length_r)
{
	if (buffer->consumed >= buffer->length)
		/* buffer is empty */
		return NULL;

	*length_r = buffer->length - buffer->consumed;
	return buffer->data + buffer->consumed;
}

void
decoder_buffer_consume(struct decoder_buffer *buffer, size_t nbytes)
{
	/* just move the "consumed" pointer - decoder_buffer_shift()
	   will do the real work later (called by
	   decoder_buffer_fill()) */
	buffer->consumed += nbytes;

	assert(buffer->consumed <= buffer->length);
}

bool
decoder_buffer_skip(struct decoder_buffer *buffer, size_t nbytes)
{
	size_t length;
	const void *data;
	bool success;

	/* this could probably be optimized by seeking */

	while (true) {
		data = decoder_buffer_read(buffer, &length);
		if (data != NULL) {
			if (length > nbytes)
				length = nbytes;
			decoder_buffer_consume(buffer, length);
			nbytes -= length;
			if (nbytes == 0)
				return true;
		}

		success = decoder_buffer_fill(buffer);
		if (!success)
			return false;
	}
}
