// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Error.hxx"

extern "C" {
#include <libavformat/avio.h>
}

#include <cstddef>
#include <utility>
#include <span>

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

	size_t Read(std::span<std::byte> dest) {
		int result = avio_read_partial(io_context,
					       reinterpret_cast<unsigned char *>(dest.data()),
					       dest.size());
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
