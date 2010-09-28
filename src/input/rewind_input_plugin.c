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
#include "input/rewind_input_plugin.h"
#include "input/curl_input_plugin.h"
#ifdef ENABLE_MMS
#include "input/mms_input_plugin.h"
#endif
#include "input_plugin.h"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <stdio.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_rewind"

struct input_rewind {
	struct input_stream input;

	/**
	 * The read position within the buffer.  Undefined as long as
	 * reading_from_buffer() returns false.
	 */
	size_t head;

	/**
	 * The write/append position within the buffer.
	 */
	size_t tail;

	/**
	 * The size of this buffer is the maximum number of bytes
	 * which can be rewinded cheaply without passing the "seek"
	 * call to CURL.
	 *
	 * The origin of this buffer is always the beginning of the
	 * stream (offset 0).
	 */
	char buffer[64 * 1024];
};

/**
 * Are we currently reading from the buffer, and does the buffer
 * contain more data for the next read operation?
 */
static bool
reading_from_buffer(const struct input_stream *is)
{
	const struct input_rewind *r = is->data;

	return r->tail > 0 && is->offset < r->input.offset;
}

/**
 * Copy public attributes from the underlying input stream to the
 * "rewind" input stream.  This function is called when a method of
 * the underlying stream has returned, which may have modified these
 * attributes.
 */
static void
copy_attributes(struct input_stream *dest)
{
	const struct input_rewind *r = dest->data;
	const struct input_stream *src = &r->input;

	dest->ready = src->ready;
	dest->seekable = src->seekable;
	dest->error = src->error;
	dest->size = src->size;
	dest->offset = src->offset;

	if (src->mime != NULL) {
		if (dest->mime != NULL)
			g_free(dest->mime);
		dest->mime = g_strdup(src->mime);
	}
}

static void
input_rewind_close(struct input_stream *is)
{
	struct input_rewind *r = is->data;

	input_stream_close(&r->input);

	g_free(r);
}

static struct tag *
input_rewind_tag(struct input_stream *is)
{
	struct input_rewind *r = is->data;

	return input_stream_tag(&r->input);
}

static int
input_rewind_buffer(struct input_stream *is)
{
	struct input_rewind *r = is->data;

	int ret = input_stream_buffer(&r->input);
	if (ret < 0 || !reading_from_buffer(is))
		copy_attributes(is);

	return ret;
}

static size_t
input_rewind_read(struct input_stream *is, void *ptr, size_t size)
{
	struct input_rewind *r = is->data;

	if (reading_from_buffer(is)) {
		/* buffered read */

		assert(r->head == (size_t)is->offset);
		assert(r->tail == (size_t)r->input.offset);

		if (size > r->tail - r->head)
			size = r->tail - r->head;

		memcpy(ptr, r->buffer + r->head, size);
		r->head += size;
		is->offset += size;

		return size;
	} else {
		/* pass method call to underlying stream */

		size_t nbytes = input_stream_read(&r->input, ptr, size);

		if (r->input.offset > (off_t)sizeof(r->buffer))
			/* disable buffering */
			r->tail = 0;
		else if (r->tail == (size_t)is->offset) {
			/* append to buffer */

			memcpy(r->buffer + r->tail, ptr, nbytes);
			r->tail += nbytes;

			assert(r->tail == (size_t)r->input.offset);
		}

		copy_attributes(is);

		return nbytes;
	}
}

static bool
input_rewind_eof(G_GNUC_UNUSED struct input_stream *is)
{
	struct input_rewind *r = is->data;

	return !reading_from_buffer(is) && input_stream_eof(&r->input);
}

static bool
input_rewind_seek(struct input_stream *is, off_t offset, int whence)
{
	struct input_rewind *r = is->data;

	assert(is->ready);

	if (whence == SEEK_SET && r->tail > 0 && offset <= (off_t)r->tail) {
		/* buffered seek */

		assert(!reading_from_buffer(is) ||
		       r->head == (size_t)is->offset);
		assert(r->tail == (size_t)r->input.offset);

		r->head = (size_t)offset;
		is->offset = offset;

		return true;
	} else {
		bool success = input_stream_seek(&r->input, offset, whence);
		copy_attributes(is);

		/* disable the buffer, because r->input has left the
		   buffered range now */
		r->tail = 0;

		return success;
	}
}

static const struct input_plugin rewind_input_plugin = {
	.close = input_rewind_close,
	.tag = input_rewind_tag,
	.buffer = input_rewind_buffer,
	.read = input_rewind_read,
	.eof = input_rewind_eof,
	.seek = input_rewind_seek,
};

void
input_rewind_open(struct input_stream *is)
{
	struct input_rewind *c;

	assert(is != NULL);
	assert(is->offset == 0);

	if (is->plugin != &input_plugin_curl
#ifdef ENABLE_MMS
	    && is->plugin != &input_plugin_mms
#endif
	   )
		/* due to limitations in the input_plugin API, we only
		   (explicitly) support the CURL input plugin */
		return;

	c = g_new(struct input_rewind, 1);
	c->tail = 0;

	/* move the CURL input stream to c->input */
	c->input = *is;
	if (is->plugin == &input_plugin_curl)
		input_curl_reinit(&c->input);

	/* convert the existing input_stream pointer to a "rewind"
	   input stream */
	is->plugin = &rewind_input_plugin;
	is->data = c;
}
