/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "lib/ffmpeg/IOContext.hxx"
#include "lib/ffmpeg/Init.hxx"
#include "lib/ffmpeg/Error.hxx"
#include "../InputStream.hxx"
#include "../InputPlugin.hxx"
#include "PluginUnavailable.hxx"

class FfmpegInputStream final : public InputStream {
	Ffmpeg::IOContext io;

	bool eof = false;

public:
	FfmpegInputStream(const char *_uri, Mutex &_mutex)
		:InputStream(_uri, _mutex),
		 io(_uri, AVIO_FLAG_READ)
	{
		seekable = (io->seekable & AVIO_SEEKABLE_NORMAL) != 0;
		size = io.GetSize();

		/* hack to make MPD select the "ffmpeg" decoder plugin
		   - since avio.h doesn't tell us the MIME type of the
		   resource, we can't select a decoder plugin, but the
		   "ffmpeg" plugin is quite good at auto-detection */
		SetMimeType("audio/x-mpd-ffmpeg");
		SetReady();
	}

	/* virtual methods from InputStream */
	bool IsEOF() noexcept override;
	size_t Read(void *ptr, size_t size) override;
	void Seek(offset_type offset) override;
};

gcc_const
static inline bool
input_ffmpeg_supported() noexcept
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
	return std::make_unique<FfmpegInputStream>(uri, mutex);
}

size_t
FfmpegInputStream::Read(void *ptr, size_t read_size)
{
	size_t result;

	{
		const ScopeUnlock unlock(mutex);
		result = io.Read(ptr, read_size);
	}

	if (result == 0) {
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
	uint64_t result;

	{
		const ScopeUnlock unlock(mutex);
		result = io.Seek(new_offset);
	}

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
