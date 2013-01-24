/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "RewindInputPlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "tag.h"

#include <glib.h>

#include <assert.h>
#include <stdio.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_rewind"

extern const struct input_plugin rewind_input_plugin;

struct RewindInputStream {
	struct input_stream base;

	struct input_stream *input;

	/**
	 * The read position within the buffer.  Undefined as long as
	 * ReadingFromBuffer() returns false.
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

	RewindInputStream(input_stream *_input)
		:input(_input), tail(0) {
		input_stream_init(&base, &rewind_input_plugin, input->uri,
				  input->mutex, input->cond);
	}

	~RewindInputStream() {
		input_stream_close(input);

		input_stream_deinit(&base);
	}

	/**
	 * Are we currently reading from the buffer, and does the
	 * buffer contain more data for the next read operation?
	 */
	bool ReadingFromBuffer() const {
		return tail > 0 && base.offset < input->offset;
	}

	/**
	 * Copy public attributes from the underlying input stream to the
	 * "rewind" input stream.  This function is called when a method of
	 * the underlying stream has returned, which may have modified these
	 * attributes.
	 */
	void CopyAttributes() {
		struct input_stream *dest = &base;
		const struct input_stream *src = input;

		assert(dest != src);
		assert(src->mime == NULL || dest->mime != src->mime);

		bool dest_ready = dest->ready;

		dest->ready = src->ready;
		dest->seekable = src->seekable;
		dest->size = src->size;
		dest->offset = src->offset;

		if (!dest_ready && src->ready) {
			g_free(dest->mime);
			dest->mime = g_strdup(src->mime);
		}
	}
};

static void
input_rewind_close(struct input_stream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	delete r;
}

static bool
input_rewind_check(struct input_stream *is, GError **error_r)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return input_stream_check(r->input, error_r);
}

static void
input_rewind_update(struct input_stream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	if (!r->ReadingFromBuffer())
		r->CopyAttributes();
}

static struct tag *
input_rewind_tag(struct input_stream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return input_stream_tag(r->input);
}

static bool
input_rewind_available(struct input_stream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return input_stream_available(r->input);
}

static size_t
input_rewind_read(struct input_stream *is, void *ptr, size_t size,
		  GError **error_r)
{
	RewindInputStream *r = (RewindInputStream *)is;

	if (r->ReadingFromBuffer()) {
		/* buffered read */

		assert(r->head == (size_t)is->offset);
		assert(r->tail == (size_t)r->input->offset);

		if (size > r->tail - r->head)
			size = r->tail - r->head;

		memcpy(ptr, r->buffer + r->head, size);
		r->head += size;
		is->offset += size;

		return size;
	} else {
		/* pass method call to underlying stream */

		size_t nbytes = input_stream_read(r->input, ptr, size, error_r);

		if (r->input->offset > (goffset)sizeof(r->buffer))
			/* disable buffering */
			r->tail = 0;
		else if (r->tail == (size_t)is->offset) {
			/* append to buffer */

			memcpy(r->buffer + r->tail, ptr, nbytes);
			r->tail += nbytes;

			assert(r->tail == (size_t)r->input->offset);
		}

		r->CopyAttributes();

		return nbytes;
	}
}

static bool
input_rewind_eof(struct input_stream *is)
{
	RewindInputStream *r = (RewindInputStream *)is;

	return !r->ReadingFromBuffer() && input_stream_eof(r->input);
}

static bool
input_rewind_seek(struct input_stream *is, goffset offset, int whence,
		  GError **error_r)
{
	RewindInputStream *r = (RewindInputStream *)is;

	assert(is->ready);

	if (whence == SEEK_SET && r->tail > 0 && offset <= (goffset)r->tail) {
		/* buffered seek */

		assert(!r->ReadingFromBuffer() ||
		       r->head == (size_t)is->offset);
		assert(r->tail == (size_t)r->input->offset);

		r->head = (size_t)offset;
		is->offset = offset;

		return true;
	} else {
		bool success = input_stream_seek(r->input, offset, whence,
						 error_r);
		r->CopyAttributes();

		/* disable the buffer, because r->input has left the
		   buffered range now */
		r->tail = 0;

		return success;
	}
}

const struct input_plugin rewind_input_plugin = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_rewind_close,
	input_rewind_check,
	input_rewind_update,
	input_rewind_tag,
	input_rewind_available,
	input_rewind_read,
	input_rewind_eof,
	input_rewind_seek,
};

struct input_stream *
input_rewind_open(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->offset == 0);

	if (is->seekable)
		/* seekable resources don't need this plugin */
		return is;

	RewindInputStream *c = new RewindInputStream(is);
	return &c->base;
}
