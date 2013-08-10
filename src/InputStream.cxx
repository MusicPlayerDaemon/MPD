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
#include "InputStream.hxx"
#include "InputRegistry.hxx"
#include "InputPlugin.hxx"
#include "input/RewindInputPlugin.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <assert.h>

static constexpr Domain input_domain("input");

struct input_stream *
input_stream_open(const char *url,
		  Mutex &mutex, Cond &cond,
		  Error &error)
{
	input_plugins_for_each_enabled(plugin) {
		struct input_stream *is;

		is = plugin->open(url, mutex, cond, error);
		if (is != NULL) {
			assert(is->plugin.close != NULL);
			assert(is->plugin.read != NULL);
			assert(is->plugin.eof != NULL);
			assert(!is->seekable || is->plugin.seek != NULL);

			is = input_rewind_open(is);

			return is;
		} else if (error.IsDefined())
			return NULL;
	}

	error.Set(input_domain, "Unrecognized URI");
	return NULL;
}

bool
input_stream_check(struct input_stream *is, Error &error)
{
	assert(is != NULL);

	return is->plugin.check == NULL ||
		is->plugin.check(is, error);
}

void
input_stream_update(struct input_stream *is)
{
	assert(is != NULL);

	if (is->plugin.update != NULL)
		is->plugin.update(is);
}

void
input_stream_wait_ready(struct input_stream *is)
{
	assert(is != NULL);

	while (true) {
		input_stream_update(is);
		if (is->ready)
			break;

		is->cond.wait(is->mutex);
	}
}

void
input_stream_lock_wait_ready(struct input_stream *is)
{
	assert(is != NULL);

	const ScopeLock protect(is->mutex);
	input_stream_wait_ready(is);
}

const char *
input_stream_get_mime_type(const struct input_stream *is)
{
	assert(is != NULL);
	assert(is->ready);

	return is->mime.empty() ? nullptr : is->mime.c_str();
}

void
input_stream_override_mime_type(struct input_stream *is, const char *mime)
{
	assert(is != NULL);
	assert(is->ready);

	is->mime = mime;
}

goffset
input_stream_get_size(const struct input_stream *is)
{
	assert(is != NULL);
	assert(is->ready);

	return is->size;
}

goffset
input_stream_get_offset(const struct input_stream *is)
{
	assert(is != NULL);
	assert(is->ready);

	return is->offset;
}

bool
input_stream_is_seekable(const struct input_stream *is)
{
	assert(is != NULL);
	assert(is->ready);

	return is->seekable;
}

bool
input_stream_cheap_seeking(const struct input_stream *is)
{
	return is->seekable && !uri_has_scheme(is->uri.c_str());
}

bool
input_stream_seek(struct input_stream *is, goffset offset, int whence,
		  Error &error)
{
	assert(is != NULL);

	if (is->plugin.seek == NULL)
		return false;

	return is->plugin.seek(is, offset, whence, error);
}

bool
input_stream_lock_seek(struct input_stream *is, goffset offset, int whence,
		       Error &error)
{
	assert(is != NULL);

	if (is->plugin.seek == NULL)
		return false;

	const ScopeLock protect(is->mutex);
	return input_stream_seek(is, offset, whence, error);
}

Tag *
input_stream_tag(struct input_stream *is)
{
	assert(is != NULL);

	return is->plugin.tag != NULL
		? is->plugin.tag(is)
		: NULL;
}

Tag *
input_stream_lock_tag(struct input_stream *is)
{
	assert(is != NULL);

	if (is->plugin.tag == NULL)
		return nullptr;

	const ScopeLock protect(is->mutex);
	return input_stream_tag(is);
}

bool
input_stream_available(struct input_stream *is)
{
	assert(is != NULL);

	return is->plugin.available != NULL
		? is->plugin.available(is)
		: true;
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size,
		  Error &error)
{
	assert(ptr != NULL);
	assert(size > 0);

	return is->plugin.read(is, ptr, size, error);
}

size_t
input_stream_lock_read(struct input_stream *is, void *ptr, size_t size,
		       Error &error)
{
	assert(ptr != NULL);
	assert(size > 0);

	const ScopeLock protect(is->mutex);
	return input_stream_read(is, ptr, size, error);
}

void input_stream_close(struct input_stream *is)
{
	is->plugin.close(is);
}

bool input_stream_eof(struct input_stream *is)
{
	return is->plugin.eof(is);
}

bool
input_stream_lock_eof(struct input_stream *is)
{
	assert(is != NULL);

	const ScopeLock protect(is->mutex);
	return input_stream_eof(is);
}

