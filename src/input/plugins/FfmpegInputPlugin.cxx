/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "FfmpegInputPlugin.hxx"
#include "lib/ffmpeg/Init.hxx"
#include "lib/ffmpeg/Domain.hxx"
#include "lib/ffmpeg/Error.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "PluginUnavailable.hxx"
#include "util/ASCII.hxx"

extern "C" {
#include <libavformat/avio.h>
}

struct FfmpegInputStream final : public InputStream {
	AVIOContext *h;

	bool eof;

	FfmpegInputStream(const char *_uri, Mutex &_mutex,
			  AVIOContext *_h)
		:InputStream(_uri, _mutex),
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
	bool IsEOF() noexcept override;
	size_t Read(void *ptr, size_t size) override;
	void Seek(offset_type offset) override;
};

static inline bool
input_ffmpeg_supported(void)
{
	void *opaque = nullptr;
	return avio_enum_protocols(&opaque, 0) != nullptr;
}

static void
input_ffmpeg_init(EventLoop &, const ConfigBlock &)
{
	FfmpegInit();

	/* disable this plugin if there's no registered protocol */
	if (!input_ffmpeg_supported())
		throw PluginUnavailable("No protocol");
}

static InputStreamPtr
input_ffmpeg_open(const char *uri,
		  Mutex &mutex)
{
	AVIOContext *h;
	auto result = avio_open(&h, uri, AVIO_FLAG_READ);
	if (result != 0)
		throw MakeFfmpegError(result);

	return std::make_unique<FfmpegInputStream>(uri, mutex, h);
}

size_t
FfmpegInputStream::Read(void *ptr, size_t read_size)
{
	int result;

	{
		const ScopeUnlock unlock(mutex);
		result = avio_read(h, (unsigned char *)ptr, read_size);
	}

	if (result <= 0) {
		if (result < 0)
			throw MakeFfmpegError(result, "avio_read() failed");

		eof = true;
		return 0;
	}

	offset += result;
	return (size_t)result;
}

bool
FfmpegInputStream::IsEOF() noexcept
{
	return eof;
}

void
FfmpegInputStream::Seek(offset_type new_offset)
{
	int64_t result;

	{
		const ScopeUnlock unlock(mutex);
		result = avio_seek(h, new_offset, SEEK_SET);
	}

	if (result < 0)
		throw MakeFfmpegError(result, "avio_seek() failed");

	offset = result;
	eof = false;
}

static constexpr const char *ffmpeg_prefixes[] = {
	"gopher://",
	"rtp://",
	"rtsp://",
	"rtmp://",
	"rtmpt://",
	"rtmps://",
	nullptr
};

const InputPlugin input_plugin_ffmpeg = {
	"ffmpeg",
	ffmpeg_prefixes,
	input_ffmpeg_init,
	nullptr,
	input_ffmpeg_open,
};
