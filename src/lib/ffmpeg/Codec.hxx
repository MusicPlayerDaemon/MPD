/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_FFMPEG_CODEC_HXX
#define MPD_FFMPEG_CODEC_HXX

#include "Error.hxx"

extern "C" {
#include <libavcodec/avcodec.h>
}

#include <new>

namespace Ffmpeg {

class CodecContext {
	AVCodecContext *codec_context = nullptr;

public:
	CodecContext() = default;

	explicit CodecContext(const AVCodec &codec)
		:codec_context(avcodec_alloc_context3(&codec))
	{
		if (codec_context == nullptr)
			throw std::bad_alloc();
	}

	~CodecContext() noexcept {
		if (codec_context != nullptr)
			avcodec_free_context(&codec_context);
	}

	CodecContext(CodecContext &&src) noexcept
		:codec_context(std::exchange(src.codec_context, nullptr)) {}

	CodecContext &operator=(CodecContext &&src) noexcept {
		using std::swap;
		swap(codec_context, src.codec_context);
		return *this;
	}

	AVCodecContext &operator*() noexcept {
		return *codec_context;
	}

	AVCodecContext *operator->() noexcept {
		return codec_context;
	}

	void FillFromParameters(const AVCodecParameters &par) {
		int err = avcodec_parameters_to_context(codec_context, &par);
		if (err < 0)
			throw MakeFfmpegError(err, "avcodec_parameters_to_context() failed");
	}

	void Open(const AVCodec &codec, AVDictionary **options) {
		int err = avcodec_open2(codec_context, &codec, options);
		if (err < 0)
			throw MakeFfmpegError(err, "avcodec_open2() failed");
	}

	void FlushBuffers() noexcept {
		avcodec_flush_buffers(codec_context);
	}
};

} // namespace Ffmpeg

#endif
