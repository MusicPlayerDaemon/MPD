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

#ifndef MPD_INPUT_STREAM_H
#define MPD_INPUT_STREAM_H

#include "check.h"

#include <glib.h>

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#if !GLIB_CHECK_VERSION(2,14,0)
typedef gint64 goffset;
#endif

struct input_stream {
	/**
	 * the plugin which implements this input stream
	 */
	const struct input_plugin *plugin;

	/**
	 * an opaque pointer managed by the plugin
	 */
	void *data;

	/**
	 * indicates whether the stream is ready for reading and
	 * whether the other attributes in this struct are valid
	 */
	bool ready;

	/**
	 * if true, then the stream is fully seekable
	 */
	bool seekable;

	/**
	 * an optional errno error code, set to non-zero after an error occured
	 */
	int error;

	/**
	 * the size of the resource, or -1 if unknown
	 */
	goffset size;

	/**
	 * the current offset within the stream
	 */
	goffset offset;

	/**
	 * the MIME content type of the resource, or NULL if unknown
	 */
	char *mime;
};

/**
 * Initializes this library and all input_stream implementations.
 */
void input_stream_global_init(void);

/**
 * Deinitializes this library and all input_stream implementations.
 */
void input_stream_global_finish(void);

/**
 * Opens a new input stream.  You may not access it until the "ready"
 * flag is set.
 *
 * @param is the input_stream object allocated by the caller
 * @return true on success
 */
bool
input_stream_open(struct input_stream *is, const char *url);

/**
 * Closes the input stream and free resources.  This does not free the
 * input_stream pointer itself, because it is assumed to be allocated
 * by the caller.
 */
void
input_stream_close(struct input_stream *is);

/**
 * Seeks to the specified position in the stream.  This will most
 * likely fail if the "seekable" flag is false.
 *
 * @param is the input_stream object
 * @param offset the relative offset
 * @param whence the base of the seek, one of SEEK_SET, SEEK_CUR, SEEK_END
 */
bool
input_stream_seek(struct input_stream *is, goffset offset, int whence);

/**
 * Returns true if the stream has reached end-of-file.
 */
bool input_stream_eof(struct input_stream *is);

/**
 * Reads the tag from the stream.
 *
 * @return a tag object which must be freed with tag_free(), or NULL
 * if the tag has not changed since the last call
 */
struct tag *
input_stream_tag(struct input_stream *is);

/**
 * Reads some of the stream into its buffer.  The following return
 * codes are defined: -1 = error, 1 = something was buffered, 0 =
 * nothing was buffered.
 *
 * The semantics of this function are not well-defined, and it will
 * eventually be removed.
 */
int input_stream_buffer(struct input_stream *is);

/**
 * Reads data from the stream into the caller-supplied buffer.
 * Returns 0 on error or eof (check with input_stream_eof()).
 *
 * @param is the input_stream object
 * @param ptr the buffer to read into
 * @param size the maximum number of bytes to read
 * @return the number of bytes read
 */
size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size);

#endif
