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
#include "input/ffmpeg_input_plugin.h"
#include "input_internal.h"
#include "input_plugin.h"

#include <libavutil/avutil.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "input_ffmpeg"

struct input_ffmpeg {
	struct input_stream base;

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	AVIOContext *h;
#else
	URLContext *h;
#endif

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
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	void *opaque = NULL;
	return avio_enum_protocols(&opaque, 0) != NULL;
#else
	return av_protocol_next(NULL) != NULL;
#endif
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
		  GMutex *mutex, GCond *cond,
		  GError **error_r)
{
	struct input_ffmpeg *i;

	if (!g_str_has_prefix(uri, "gopher://") &&
	    !g_str_has_prefix(uri, "rtp://") &&
	    !g_str_has_prefix(uri, "rtsp://") &&
	    !g_str_has_prefix(uri, "rtmp://") &&
	    !g_str_has_prefix(uri, "rtmpt://") &&
	    !g_str_has_prefix(uri, "rtmps://"))
		return NULL;

	i = g_new(struct input_ffmpeg, 1);
	input_stream_init(&i->base, &input_plugin_ffmpeg, uri,
			  mutex, cond);

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,1,0)
	int ret = avio_open(&i->h, uri, AVIO_FLAG_READ);
#elif LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	int ret = avio_open(&i->h, uri, AVIO_RDONLY);
#else
	int ret = url_open(&i->h, uri, URL_RDONLY);
#endif
	if (ret != 0) {
		g_free(i);
		g_set_error(error_r, ffmpeg_quark(), ret,
			    "libavformat failed to open the URI");
		return NULL;
	}

	i->eof = false;

	i->base.ready = true;
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	i->base.seekable = (i->h->seekable & AVIO_SEEKABLE_NORMAL) != 0;
	i->base.size = avio_size(i->h);
#else
	i->base.seekable = !i->h->is_streamed;
	i->base.size = url_filesize(i->h);
#endif

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

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	int ret = avio_read(i->h, ptr, size);
#else
	int ret = url_read(i->h, ptr, size);
#endif
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

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	avio_close(i->h);
#else
	url_close(i->h);
#endif
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
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(53,0,0)
	int64_t ret = avio_seek(i->h, offset, whence);
#else
	int64_t ret = url_seek(i->h, offset, whence);
#endif

	if (ret >= 0) {
		i->eof = false;
		return true;
	} else {
		g_set_error(error_r, ffmpeg_quark(), 0, "url_seek() failed");
		return false;
	}
}

const struct input_plugin input_plugin_ffmpeg = {
	.name = "ffmpeg",
	.init = input_ffmpeg_init,
	.open = input_ffmpeg_open,
	.close = input_ffmpeg_close,
	.read = input_ffmpeg_read,
	.eof = input_ffmpeg_eof,
	.seek = input_ffmpeg_seek,
};
