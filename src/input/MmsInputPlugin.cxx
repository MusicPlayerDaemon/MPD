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
#include "MmsInputPlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"

#include <glib.h>
#include <libmms/mmsx.h>

#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_mms"

struct MmsInputStream {
	struct input_stream base;

	mmsx_t *mms;

	bool eof;

	MmsInputStream(const char *uri,
		       Mutex &mutex, Cond &cond,
		       mmsx_t *_mms)
		:mms(_mms), eof(false) {
		input_stream_init(&base, &input_plugin_mms, uri, mutex, cond);

		/* XX is this correct?  at least this selects the ffmpeg
		   decoder, which seems to work fine*/
		base.mime = g_strdup("audio/x-ms-wma");

		base.ready = true;
	}

	~MmsInputStream() {
		mmsx_close(mms);
		input_stream_deinit(&base);
	}
};

static inline GQuark
mms_quark(void)
{
	return g_quark_from_static_string("mms");
}

static struct input_stream *
input_mms_open(const char *url,
	       Mutex &mutex, Cond &cond,
	       GError **error_r)
{
	if (!g_str_has_prefix(url, "mms://") &&
	    !g_str_has_prefix(url, "mmsh://") &&
	    !g_str_has_prefix(url, "mmst://") &&
	    !g_str_has_prefix(url, "mmsu://"))
		return nullptr;

	const auto mms = mmsx_connect(nullptr, nullptr, url, 128 * 1024);
	if (mms == nullptr) {
		g_set_error(error_r, mms_quark(), 0, "mmsx_connect() failed");
		return nullptr;
	}

	auto m = new MmsInputStream(url, mutex, cond, mms);
	return &m->base;
}

static size_t
input_mms_read(struct input_stream *is, void *ptr, size_t size,
	       GError **error_r)
{
	MmsInputStream *m = (MmsInputStream *)is;
	int ret;

	ret = mmsx_read(nullptr, m->mms, (char *)ptr, size);
	if (ret <= 0) {
		if (ret < 0) {
			g_set_error(error_r, mms_quark(), errno,
				    "mmsx_read() failed: %s",
				    g_strerror(errno));
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
	MmsInputStream *m = (MmsInputStream *)is;

	delete m;
}

static bool
input_mms_eof(struct input_stream *is)
{
	MmsInputStream *m = (MmsInputStream *)is;

	return m->eof;
}

static bool
input_mms_seek(G_GNUC_UNUSED struct input_stream *is,
	       G_GNUC_UNUSED goffset offset, G_GNUC_UNUSED int whence,
	       G_GNUC_UNUSED GError **error_r)
{
	return false;
}

const struct input_plugin input_plugin_mms = {
	"mms",
	nullptr,
	nullptr,
	input_mms_open,
	input_mms_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_mms_read,
	input_mms_eof,
	input_mms_seek,
};
