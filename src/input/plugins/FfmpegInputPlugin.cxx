/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "lib/ffmpeg/Domain.hxx"
#include "lib/ffmpeg/Error.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"

extern "C" {
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

struct FfmpegInputStream final : public InputStream {
	AVIOContext *h;

	bool eof;

	FfmpegInputStream(const char *_uri, Mutex &_mutex, Cond &_cond,
			  AVIOContext *_h)
		:InputStream(_uri, _mutex, _cond),
		 h(_h), eof(false) {
		seekable = (h->seekable & AVIO_SEEKABLE_NORMAL) != 0;
		size = avio_size(h);

		/* hack to make MPD select the "ffmpeg" decoder plugin
		   - since avio.h doesn't tell us the MIME type of the
		   resource, we can't select a decoder plugin, but the
		   "ffmpeg" plugin is quite good at auto-detection */
		SetMimeType("audio/x-mpd-ffmpeg");
		SetReady();
	}

	~FfmpegInputStream() {
		avio_close(h);
	}

	/* virtual methods from InputStream */
	bool IsEOF() override;
	size_t Read(void *ptr, size_t size, Error &error) override;
	bool Seek(offset_type offset, Error &error) override;
};

static inline bool
input_ffmpeg_supported(void)
{
	void *opaque = nullptr;
	return avio_enum_protocols(&opaque, 0) != nullptr;
}

static InputPlugin::InitResult
input_ffmpeg_init(gcc_unused const config_param &param,
		  Error &error)
{
	av_register_all();

	/* disable this plugin if there's no registered protocol */
	if (!input_ffmpeg_supported()) {
		error.Set(ffmpeg_domain, "No protocol");
		return InputPlugin::InitResult::UNAVAILABLE;
	}

	return InputPlugin::InitResult::SUCCESS;
}

static InputStream *
input_ffmpeg_open(const char *uri,
		  Mutex &mutex, Cond &cond,
		  Error &error)
{
	if (!StringStartsWith(uri, "gopher://") &&
	    !StringStartsWith(uri, "rtp://") &&
	    !StringStartsWith(uri, "rtsp://") &&
	    !StringStartsWith(uri, "rtmp://") &&
	    !StringStartsWith(uri, "rtmpt://") &&
	    !StringStartsWith(uri, "rtmps://"))
		return nullptr;

	AVIOContext *h;
	auto result = avio_open(&h, uri, AVIO_FLAG_READ);
	if (result != 0) {
		SetFfmpegError(error, result);
		return nullptr;
	}

	return new FfmpegInputStream(uri, mutex, cond, h);
}

size_t
FfmpegInputStream::Read(void *ptr, size_t read_size, Error &error)
{
	auto result = avio_read(h, (unsigned char *)ptr, read_size);
	if (result <= 0) {
		if (result < 0)
			SetFfmpegError(error, result, "avio_read() failed");

		eof = true;
		return false;
	}

	offset += result;
	return (size_t)result;
}

bool
FfmpegInputStream::IsEOF()
{
	return eof;
}

bool
FfmpegInputStream::Seek(offset_type new_offset, Error &error)
{
	auto result = avio_seek(h, new_offset, SEEK_SET);

	if (result < 0) {
		SetFfmpegError(error, result, "avio_seek() failed");
		return false;
	}

	offset = result;
	eof = false;
	return true;
}

const InputPlugin input_plugin_ffmpeg = {
	"ffmpeg",
	input_ffmpeg_init,
	nullptr,
	input_ffmpeg_open,
};
