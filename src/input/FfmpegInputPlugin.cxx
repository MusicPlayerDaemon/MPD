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

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "config.h"
#include "FfmpegInputPlugin.hxx"
#include "InputInternal.hxx"
#include "InputStream.hxx"
#include "InputPlugin.hxx"

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_ffmpeg"

struct input_ffmpeg {
	struct input_stream base;

	AVIOContext *h;

	bool eof;
};

static inline GQuark
ffmpeg_quark(void)
{
	return g_quark_from_static_string("ffmpeg");
}

static inline bool
input_ffmpeg_supported(void)
{
	void *opaque = nullptr;
	return avio_enum_protocols(&opaque, 0) != nullptr;
}

static bool
input_ffmpeg_init(G_GNUC_UNUSED const struct config_param *param,
		  G_GNUC_UNUSED GError **error_r)
{
	av_register_all();

	/* disable this plugin if there's no registered protocol */
	if (!input_ffmpeg_supported()) {
		g_set_error(error_r, ffmpeg_quark(), 0,
			    "No protocol");
		return false;
	}

	return true;
}

static struct input_stream *
input_ffmpeg_open(const char *uri,
		  Mutex &mutex, Cond &cond,
		  GError **error_r)
{
	struct input_ffmpeg *i;

	if (!g_str_has_prefix(uri, "gopher://") &&
	    !g_str_has_prefix(uri, "rtp://") &&
	    !g_str_has_prefix(uri, "rtsp://") &&
	    !g_str_has_prefix(uri, "rtmp://") &&
	    !g_str_has_prefix(uri, "rtmpt://") &&
	    !g_str_has_prefix(uri, "rtmps://"))
		return nullptr;

	i = g_new(struct input_ffmpeg, 1);
	input_stream_init(&i->base, &input_plugin_ffmpeg, uri,
			  mutex, cond);

	int ret = avio_open(&i->h, uri, AVIO_FLAG_READ);
	if (ret != 0) {
		g_free(i);
		g_set_error(error_r, ffmpeg_quark(), ret,
			    "libavformat failed to open the URI");
		return nullptr;
	}

	i->eof = false;

	i->base.ready = true;
	i->base.seekable = (i->h->seekable & AVIO_SEEKABLE_NORMAL) != 0;
	i->base.size = avio_size(i->h);

	/* hack to make MPD select the "ffmpeg" decoder plugin - since
	   avio.h doesn't tell us the MIME type of the resource, we
	   can't select a decoder plugin, but the "ffmpeg" plugin is
	   quite good at auto-detection */
	i->base.mime = g_strdup("audio/x-mpd-ffmpeg");

	return &i->base;
}

static size_t
input_ffmpeg_read(struct input_stream *is, void *ptr, size_t size,
		  GError **error_r)
{
	struct input_ffmpeg *i = (struct input_ffmpeg *)is;

	int ret = avio_read(i->h, (unsigned char *)ptr, size);
	if (ret <= 0) {
		if (ret < 0)
			g_set_error(error_r, ffmpeg_quark(), 0,
				    "url_read() failed");

		i->eof = true;
		return false;
	}

	is->offset += ret;
	return (size_t)ret;
}

static void
input_ffmpeg_close(struct input_stream *is)
{
	struct input_ffmpeg *i = (struct input_ffmpeg *)is;

	avio_close(i->h);
	input_stream_deinit(&i->base);
	g_free(i);
}

static bool
input_ffmpeg_eof(struct input_stream *is)
{
	struct input_ffmpeg *i = (struct input_ffmpeg *)is;

	return i->eof;
}

static bool
input_ffmpeg_seek(struct input_stream *is, goffset offset, int whence,
		  G_GNUC_UNUSED GError **error_r)
{
	struct input_ffmpeg *i = (struct input_ffmpeg *)is;
	int64_t ret = avio_seek(i->h, offset, whence);

	if (ret >= 0) {
		i->eof = false;
		return true;
	} else {
		g_set_error(error_r, ffmpeg_quark(), 0, "url_seek() failed");
		return false;
	}
}

const struct input_plugin input_plugin_ffmpeg = {
	"ffmpeg",
	input_ffmpeg_init,
	nullptr,
	input_ffmpeg_open,
	input_ffmpeg_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	input_ffmpeg_read,
	input_ffmpeg_eof,
	input_ffmpeg_seek,
};
