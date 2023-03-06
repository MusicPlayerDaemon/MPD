// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_FORMAT_HXX
#define MPD_FFMPEG_FORMAT_HXX

#include "Error.hxx"

extern "C" {
#include <libavformat/avformat.h>
}

#include <new>

namespace Ffmpeg {

class FormatContext {
	AVFormatContext *format_context = nullptr;

public:
	FormatContext() = default;

	explicit FormatContext(AVIOContext *pb)
		:format_context(avformat_alloc_context())
	{
		if (format_context == nullptr)
			throw std::bad_alloc();

		format_context->pb = pb;
	}

	~FormatContext() noexcept {
		if (format_context != nullptr)
			avformat_close_input(&format_context);
	}

	FormatContext(FormatContext &&src) noexcept
		:format_context(std::exchange(src.format_context, nullptr)) {}

	FormatContext &operator=(FormatContext &&src) noexcept {
		using std::swap;
		swap(format_context, src.format_context);
		return *this;
	}

	void OpenInput(const char *url, AVInputFormat *fmt,
		       AVDictionary **options) {
		int err = avformat_open_input(&format_context, url, fmt,
					      options);
		if (err < 0)
			throw MakeFfmpegError(err, "avformat_open_input() failed");
	}

	AVFormatContext &operator*() noexcept {
		return *format_context;
	}

	AVFormatContext *operator->() noexcept {
		return format_context;
	}
};

} // namespace Ffmpeg

#endif
