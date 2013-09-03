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

#ifndef MPD_INPUT_LEGACY_HXX
#define MPD_INPUT_LEGACY_HXX

#include "check.h"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "gcc.h"

#include <glib.h>

#include <stddef.h>

struct Tag;
struct input_stream;

/**
 * Opens a new input stream.  You may not access it until the "ready"
 * flag is set.
 *
 * @param mutex a mutex that is used to protect this object; must be
 * locked before calling any of the public methods
 * @param cond a cond that gets signalled when the state of
 * this object changes; may be NULL if the caller doesn't want to get
 * notifications
 * @return an #input_stream object on success, NULL on error
 */
gcc_nonnull(1)
gcc_malloc
struct input_stream *
input_stream_open(const char *uri,
		  Mutex &mutex, Cond &cond,
		  GError **error_r);

/**
 * Close the input stream and free resources.
 *
 * The caller must not lock the mutex.
 */
gcc_nonnull(1)
void
input_stream_close(struct input_stream *is);

/**
 * Check for errors that may have occurred in the I/O thread.
 *
 * @return false on error
 */
gcc_nonnull(1)
bool
input_stream_check(struct input_stream *is, GError **error_r);

/**
 * Update the public attributes.  Call before accessing attributes
 * such as "ready" or "offset".
 */
gcc_nonnull(1)
void
input_stream_update(struct input_stream *is);

/**
 * Wait until the stream becomes ready.
 *
 * The caller must lock the mutex.
 */
gcc_nonnull(1)
void
input_stream_wait_ready(struct input_stream *is);

/**
 * Wrapper for input_stream_wait_locked() which locks and unlocks the
 * mutex; the caller must not be holding it already.
 */
gcc_nonnull(1)
void
input_stream_lock_wait_ready(struct input_stream *is);

gcc_nonnull_all gcc_pure
const char *
input_stream_get_mime_type(const struct input_stream *is);

gcc_nonnull_all
void
input_stream_override_mime_type(struct input_stream *is, const char *mime);

gcc_nonnull_all gcc_pure
goffset
input_stream_get_size(const struct input_stream *is);

gcc_nonnull_all gcc_pure
goffset
input_stream_get_offset(const struct input_stream *is);

gcc_nonnull_all gcc_pure
bool
input_stream_is_seekable(const struct input_stream *is);

/**
 * Determines whether seeking is cheap.  This is true for local files.
 */
gcc_pure gcc_nonnull(1)
bool
input_stream_cheap_seeking(const struct input_stream *is);

/**
 * Seeks to the specified position in the stream.  This will most
 * likely fail if the "seekable" flag is false.
 *
 * The caller must lock the mutex.
 *
 * @param is the input_stream object
 * @param offset the relative offset
 * @param whence the base of the seek, one of SEEK_SET, SEEK_CUR, SEEK_END
 */
gcc_nonnull(1)
bool
input_stream_seek(struct input_stream *is, goffset offset, int whence,
		  GError **error_r);

/**
 * Wrapper for input_stream_seek() which locks and unlocks the
 * mutex; the caller must not be holding it already.
 */
gcc_nonnull(1)
bool
input_stream_lock_seek(struct input_stream *is, goffset offset, int whence,
		       GError **error_r);

/**
 * Returns true if the stream has reached end-of-file.
 *
 * The caller must lock the mutex.
 */
gcc_nonnull(1)
gcc_pure
bool input_stream_eof(struct input_stream *is);

/**
 * Wrapper for input_stream_eof() which locks and unlocks the mutex;
 * the caller must not be holding it already.
 */
gcc_nonnull(1)
gcc_pure
bool
input_stream_lock_eof(struct input_stream *is);

/**
 * Reads the tag from the stream.
 *
 * The caller must lock the mutex.
 *
 * @return a tag object which must be freed by the caller, or nullptr
 * if the tag has not changed since the last call
 */
gcc_nonnull(1)
gcc_malloc
Tag *
input_stream_tag(struct input_stream *is);

/**
 * Wrapper for input_stream_tag() which locks and unlocks the
 * mutex; the caller must not be holding it already.
 */
gcc_nonnull(1)
gcc_malloc
Tag *
input_stream_lock_tag(struct input_stream *is);

/**
 * Returns true if the next read operation will not block: either data
 * is available, or end-of-stream has been reached, or an error has
 * occurred.
 *
 * The caller must lock the mutex.
 */
gcc_nonnull(1)
gcc_pure
bool
input_stream_available(struct input_stream *is);

/**
 * Reads data from the stream into the caller-supplied buffer.
 * Returns 0 on error or eof (check with input_stream_eof()).
 *
 * The caller must lock the mutex.
 *
 * @param is the input_stream object
 * @param ptr the buffer to read into
 * @param size the maximum number of bytes to read
 * @return the number of bytes read
 */
gcc_nonnull(1, 2)
size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size,
		  GError **error_r);

/**
 * Wrapper for input_stream_tag() which locks and unlocks the
 * mutex; the caller must not be holding it already.
 */
gcc_nonnull(1, 2)
size_t
input_stream_lock_read(struct input_stream *is, void *ptr, size_t size,
		       GError **error_r);

#endif
