// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_BUFFER_HXX
#define MPD_FFMPEG_BUFFER_HXX

extern "C" {
#include <libavutil/mem.h>
}

#include <cstddef>

class FfmpegBuffer {
	void *data = nullptr;
	unsigned size = 0;

public:
	FfmpegBuffer() noexcept = default;

	~FfmpegBuffer() noexcept {
		av_free(data);
	}

	FfmpegBuffer(const FfmpegBuffer &) = delete;
	FfmpegBuffer &operator=(const FfmpegBuffer &) = delete;

	[[gnu::malloc]]
	void *Get(size_t min_size) noexcept {
		av_fast_malloc(&data, &size, min_size);
		return data;
	}

	template<typename T>
	T *GetT(size_t n) noexcept {
		return (T *)Get(n * sizeof(T));
	}
};

#endif
