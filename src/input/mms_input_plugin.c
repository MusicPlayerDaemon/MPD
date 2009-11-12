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
#include "input/mms_input_plugin.h"
#include "input_plugin.h"

#include <glib.h>
#include <libmms/mmsx.h>

#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_mms"

struct input_mms {
	mmsx_t *mms;

	bool eof;
};

static bool
input_mms_open(struct input_stream *is, const char *url)
{
	struct input_mms *m;

	if (!g_str_has_prefix(url, "mms://") &&
	    !g_str_has_prefix(url, "mmsh://") &&
	    !g_str_has_prefix(url, "mmst://") &&
	    !g_str_has_prefix(url, "mmsu://"))
		return false;

	m = g_new(struct input_mms, 1);
	m->mms = mmsx_connect(NULL, NULL, url, 128 * 1024);
	if (m->mms == NULL) {
		g_warning("mmsx_connect() failed");
		return false;
	}

	/* XX is this correct?  at least this selects the ffmpeg
	   decoder, which seems to work fine*/
	is->mime = g_strdup("audio/x-ms-wma");

	is->plugin = &input_plugin_mms;
	is->data = m;
	is->ready = true;
	return true;
}

static size_t
input_mms_read(struct input_stream *is, void *ptr, size_t size)
{
	struct input_mms *m = is->data;
	int ret;

	ret = mmsx_read(NULL, m->mms, ptr, size);
	if (ret <= 0) {
		if (ret < 0) {
			is->error = errno;
			g_warning("mmsx_read() failed: %s", g_strerror(errno));
		}

		m->eof = true;
		return false;
	}

	is->offset += ret;

	return (size_t)ret;
}

static void
input_mms_close(struct input_stream *is)
{
	struct input_mms *m = is->data;

	mmsx_close(m->mms);
	g_free(m);
}

static bool
input_mms_eof(struct input_stream *is)
{
	struct input_mms *m = is->data;

	return m->eof;
}

static int
input_mms_buffer(G_GNUC_UNUSED struct input_stream *is)
{
	return 0;
}

static bool
input_mms_seek(G_GNUC_UNUSED struct input_stream *is,
	       G_GNUC_UNUSED goffset offset, G_GNUC_UNUSED int whence)
{
	return false;
}

const struct input_plugin input_plugin_mms = {
	.name = "mms",
	.open = input_mms_open,
	.close = input_mms_close,
	.buffer = input_mms_buffer,
	.read = input_mms_read,
	.eof = input_mms_eof,
	.seek = input_mms_seek,
};
