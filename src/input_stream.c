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
#include "input_stream.h"
#include "input_registry.h"
#include "input_plugin.h"

#include <glib.h>
#include <assert.h>

static inline GQuark
input_quark(void)
{
	return g_quark_from_static_string("input");
}

bool
input_stream_open(struct input_stream *is, const char *url, GError **error_r)
{
	GError *error = NULL;

	assert(error_r == NULL || *error_r == NULL);

	is->seekable = false;
	is->ready = false;
	is->offset = 0;
	is->size = -1;
	is->mime = NULL;

	for (unsigned i = 0; input_plugins[i] != NULL; ++i) {
		const struct input_plugin *plugin = input_plugins[i];

		if (!input_plugins_enabled[i])
			continue;

		if (plugin->open(is, url, &error)) {
			assert(is->plugin != NULL);
			assert(is->plugin->close != NULL);
			assert(is->plugin->read != NULL);
			assert(is->plugin->eof != NULL);
			assert(!is->seekable || is->plugin->seek != NULL);

			return true;
		} else if (error != NULL) {
			g_propagate_error(error_r, error);
			return false;
		}
	}

	g_set_error(error_r, input_quark(), 0, "Unrecognized URI");
	return false;
}

bool
input_stream_seek(struct input_stream *is, goffset offset, int whence,
		  GError **error_r)
{
	if (is->plugin->seek == NULL)
		return false;

	return is->plugin->seek(is, offset, whence, error_r);
}

struct tag *
input_stream_tag(struct input_stream *is)
{
	assert(is != NULL);

	return is->plugin->tag != NULL
		? is->plugin->tag(is)
		: NULL;
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size,
		  GError **error_r)
{
	assert(ptr != NULL);
	assert(size > 0);

	return is->plugin->read(is, ptr, size, error_r);
}

void input_stream_close(struct input_stream *is)
{
	is->plugin->close(is);

	g_free(is->mime);
}

bool input_stream_eof(struct input_stream *is)
{
	return is->plugin->eof(is);
}

int
input_stream_buffer(struct input_stream *is, GError **error_r)
{
	if (is->plugin->buffer == NULL)
		return 0;

	return is->plugin->buffer(is, error_r);
}
