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
