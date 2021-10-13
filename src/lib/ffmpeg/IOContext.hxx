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

#ifndef MPD_FFMPEG_IO_CONTEXT_HXX
#define MPD_FFMPEG_IO_CONTEXT_HXX

#include "Error.hxx"

extern "C" {
#include <libavformat/avio.h>
}

#include <utility>

namespace Ffmpeg {

class IOContext {
	AVIOContext *io_context = nullptr;

public:
	IOContext() = default;

	IOContext(const char *url, int flags) {
		int err = avio_open(&io_context, url, flags);
		if (err < 0)
			throw MakeFfmpegError(err);
	}

	~IOContext() noexcept {
		if (io_context != nullptr)
			avio_close(io_context);
	}

	IOContext(IOContext &&src) noexcept
		:io_context(std::exchange(src.io_context, nullptr)) {}

	IOContext &operator=(IOContext &&src) noexcept {
		using std::swap;
		swap(io_context, src.io_context);
		return *this;
	}

	AVIOContext &operator*() noexcept {
		return *io_context;
	}

	AVIOContext *operator->() noexcept {
		return io_context;
	}

	[[gnu::pure]]
	auto GetSize() const noexcept {
		return avio_size(io_context);
	}

	[[gnu::pure]]
	bool IsEOF() const noexcept {
		return avio_feof(io_context) != 0;
	}

	size_t Read(void *buffer, size_t size) {
#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(57, 81, 100)
		int result = avio_read_partial(io_context,
					       (unsigned char *)buffer, size);
#else
		int result = avio_read(io_context,
				       (unsigned char *)buffer, size);
#endif
		if (result < 0)
			throw MakeFfmpegError(result, "avio_read() failed");

		return result;
	}

	uint64_t Seek(uint64_t offset) {
		int64_t result = avio_seek(io_context, offset, SEEK_SET);
		if (result < 0)
			throw MakeFfmpegError(result, "avio_seek() failed");

		return result;
	}
};

} // namespace Ffmpeg

#endif
