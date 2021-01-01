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
