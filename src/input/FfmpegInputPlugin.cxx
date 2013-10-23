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
#include "InputStream.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

#include <glib.h>

struct FfmpegInputStream {
	struct input_stream base;

	AVIOContext *h;

	bool eof;

	FfmpegInputStream(const char *uri, Mutex &mutex, Cond &cond,
			  AVIOContext *_h)
		:base(input_plugin_ffmpeg, uri, mutex, cond),
		 h(_h), eof(false) {
		base.ready = true;
		base.seekable = (h->seekable & AVIO_SEEKABLE_NORMAL) != 0;
		base.size = avio_size(h);

		/* hack to make MPD select the "ffmpeg" decoder plugin
		   - since avio.h doesn't tell us the MIME type of the
		   resource, we can't select a decoder plugin, but the
		   "ffmpeg" plugin is quite good at auto-detection */
		base.mime = "audio/x-mpd-ffmpeg";
	}

	~FfmpegInputStream() {
		avio_close(h);
	}
};

static constexpr Domain ffmpeg_domain("ffmpeg");

static inline bool
input_ffmpeg_supported(void)
{
	void *opaque = nullptr;
	return avio_enum_protocols(&opaque, 0) != nullptr;
}

static bool
input_ffmpeg_init(gcc_unused const config_param &param,
		  Error &error)
{
	av_register_all();

	/* disable this plugin if there's no registered protocol */
	if (!input_ffmpeg_supported()) {
		error.Set(ffmpeg_domain, "No protocol");
		return false;
	}

	return true;
}

static struct input_stream *
input_ffmpeg_open(const char *uri,
		  Mutex &mutex, Cond &cond,
		  Error &error)
{
	if (!g_str_has_prefix(uri, "gopher://") &&
	    !g_str_has_prefix(uri, "rtp://") &&
	    !g_str_has_prefix(uri, "rtsp://") &&
	    !g_str_has_prefix(uri, "rtmp://") &&
	    !g_str_has_prefix(uri, "rtmpt://") &&
	    !g_str_has_prefix(uri, "rtmps://"))
		return nullptr;

	AVIOContext *h;
	int ret = avio_open(&h, uri, AVIO_FLAG_READ);
	if (ret != 0) {
		error.Set(ffmpeg_domain, ret,
			  "libavformat failed to open the URI");
		return nullptr;
	}

	auto *i = new FfmpegInputStream(uri, mutex, cond, h);
	return &i->base;
}

static size_t
input_ffmpeg_read(struct input_stream *is, void *ptr, size_t size,
		  Error &error)
{
	FfmpegInputStream *i = (FfmpegInputStream *)is;

	int ret = avio_read(i->h, (unsigned char *)ptr, size);
	if (ret <= 0) {
		if (ret < 0)
			error.Set(ffmpeg_domain, "avio_read() failed");

		i->eof = true;
		return false;
	}

	is->offset += ret;
	return (size_t)ret;
}

static void
input_ffmpeg_close(struct input_stream *is)
{
	FfmpegInputStream *i = (FfmpegInputStream *)is;

	delete i;
}

static bool
input_ffmpeg_eof(struct input_stream *is)
{
	FfmpegInputStream *i = (FfmpegInputStream *)is;

	return i->eof;
}

static bool
input_ffmpeg_seek(struct input_stream *is, InputPlugin::offset_type offset,
		  int whence,
		  Error &error)
{
	FfmpegInputStream *i = (FfmpegInputStream *)is;
	int64_t ret = avio_seek(i->h, offset, whence);

	if (ret >= 0) {
		i->eof = false;
		return true;
	} else {
		error.Set(ffmpeg_domain, "avio_seek() failed");
		return false;
	}
}

const InputPlugin input_plugin_ffmpeg = {
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
