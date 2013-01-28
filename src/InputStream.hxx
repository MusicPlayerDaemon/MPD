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

#ifndef MPD_INPUT_STREAM_HXX
#define MPD_INPUT_STREAM_HXX

#include "input_stream.h"
#include "check.h"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "gcc.h"

#include <glib.h>

#include <assert.h>

struct input_stream {
	/**
	 * the plugin which implements this input stream
	 */
	const struct input_plugin *plugin;

	/**
	 * The absolute URI which was used to open this stream.  May
	 * be NULL if this is unknown.
	 */
	char *uri;

	/**
	 * A mutex that protects the mutable attributes of this object
	 * and its implementation.  It must be locked before calling
	 * any of the public methods.
	 *
	 * This object is allocated by the client, and the client is
	 * responsible for freeing it.
	 */
	Mutex *mutex;

	/**
	 * A cond that gets signalled when the state of this object
	 * changes from the I/O thread.  The client of this object may
	 * wait on it.  Optional, may be NULL.
	 *
	 * This object is allocated by the client, and the client is
	 * responsible for freeing it.
	 */
	Cond *cond;

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

	input_stream(const input_plugin &_plugin,
		     const char *_uri, Mutex &_mutex, Cond &_cond)
		:plugin(&_plugin), uri(g_strdup(_uri)),
		 mutex(&_mutex), cond(&_cond),
		 ready(false), seekable(false),
		 size(-1), offset(0),
		 mime(nullptr) {
		assert(_uri != NULL);
	}

	~input_stream() {
		g_free(uri);
		g_free(mime);
	}
};

gcc_nonnull(1)
static inline void
input_stream_lock(struct input_stream *is)
{
	is->mutex->lock();
}

gcc_nonnull(1)
static inline void
input_stream_unlock(struct input_stream *is)
{
	is->mutex->unlock();
}

#endif
