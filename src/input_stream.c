/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "input_stream.h"
#include "input_registry.h"
#include "input_plugin.h"
#include "input/rewind_input_plugin.h"

#include <glib.h>
#include <assert.h>

static inline GQuark
input_quark(void)
{
	return g_quark_from_static_string("input");
}

struct input_stream *
input_stream_open(const char *url,
		  GMutex *mutex, GCond *cond,
		  GError **error_r)
{
	GError *error = NULL;

	assert(mutex != NULL);
	assert(error_r == NULL || *error_r == NULL);

	input_plugins_for_each_enabled(plugin) {
		struct input_stream *is;

		is = plugin->open(url, mutex, cond, &error);
		if (is != NULL) {
			assert(is->plugin != NULL);
			assert(is->plugin->close != NULL);
			assert(is->plugin->read != NULL);
			assert(is->plugin->eof != NULL);
			assert(!is->seekable || is->plugin->seek != NULL);

			is = input_rewind_open(is);

			return is;
		} else if (error != NULL) {
			g_propagate_error(error_r, error);
			return NULL;
		}
	}

	g_set_error(error_r, input_quark(), 0, "Unrecognized URI");
	return NULL;
}

bool
input_stream_check(struct input_stream *is, GError **error_r)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	return is->plugin->check == NULL ||
		is->plugin->check(is, error_r);
}

void
input_stream_update(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	if (is->plugin->update != NULL)
		is->plugin->update(is);
}

void
input_stream_wait_ready(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->mutex != NULL);
	assert(is->cond != NULL);

	while (true) {
		input_stream_update(is);
		if (is->ready)
			break;

		g_cond_wait(is->cond, is->mutex);
	}
}

void
input_stream_lock_wait_ready(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->mutex != NULL);
	assert(is->cond != NULL);

	g_mutex_lock(is->mutex);
	input_stream_wait_ready(is);
	g_mutex_unlock(is->mutex);
}

bool
input_stream_seek(struct input_stream *is, goffset offset, int whence,
		  GError **error_r)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	if (is->plugin->seek == NULL)
		return false;

	return is->plugin->seek(is, offset, whence, error_r);
}

bool
input_stream_lock_seek(struct input_stream *is, goffset offset, int whence,
		       GError **error_r)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	if (is->plugin->seek == NULL)
		return false;

	if (is->mutex == NULL)
		/* no locking */
		return input_stream_seek(is, offset, whence, error_r);

	g_mutex_lock(is->mutex);
	bool success = input_stream_seek(is, offset, whence, error_r);
	g_mutex_unlock(is->mutex);
	return success;
}

struct tag *
input_stream_tag(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	return is->plugin->tag != NULL
		? is->plugin->tag(is)
		: NULL;
}

struct tag *
input_stream_lock_tag(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	if (is->plugin->tag == NULL)
		return false;

	if (is->mutex == NULL)
		/* no locking */
		return input_stream_tag(is);

	g_mutex_lock(is->mutex);
	struct tag *tag = input_stream_tag(is);
	g_mutex_unlock(is->mutex);
	return tag;
}

bool
input_stream_available(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	return is->plugin->available != NULL
		? is->plugin->available(is)
		: true;
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size,
		  GError **error_r)
{
	assert(ptr != NULL);
	assert(size > 0);

	return is->plugin->read(is, ptr, size, error_r);
}

size_t
input_stream_lock_read(struct input_stream *is, void *ptr, size_t size,
		       GError **error_r)
{
	assert(ptr != NULL);
	assert(size > 0);

	if (is->mutex == NULL)
		/* no locking */
		return input_stream_read(is, ptr, size, error_r);

	g_mutex_lock(is->mutex);
	size_t nbytes = input_stream_read(is, ptr, size, error_r);
	g_mutex_unlock(is->mutex);
	return nbytes;
}

void input_stream_close(struct input_stream *is)
{
	is->plugin->close(is);
}

bool input_stream_eof(struct input_stream *is)
{
	return is->plugin->eof(is);
}

bool
input_stream_lock_eof(struct input_stream *is)
{
	assert(is != NULL);
	assert(is->plugin != NULL);

	if (is->mutex == NULL)
		/* no locking */
		return input_stream_eof(is);

	g_mutex_lock(is->mutex);
	bool eof = input_stream_eof(is);
	g_mutex_unlock(is->mutex);
	return eof;
}

