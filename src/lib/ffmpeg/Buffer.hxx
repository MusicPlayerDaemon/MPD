/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(52, 18, 0)
#define HAVE_AV_FAST_MALLOC
#else
#include <libavcodec/avcodec.h>
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(52, 25, 0)
#define HAVE_AV_FAST_MALLOC
#endif
#endif
}

#include <stddef.h>

/* suppress the ffmpeg compatibility macro */
#ifdef SampleFormat
#undef SampleFormat
#endif

class FfmpegBuffer {
	void *data;
	unsigned size;

public:
	FfmpegBuffer():data(nullptr), size(0) {}

	~FfmpegBuffer() {
		av_free(data);
	}

	gcc_malloc
	void *Get(size_t min_size) {
#ifdef HAVE_AV_FAST_MALLOC
		av_fast_malloc(&data, &size, min_size);
#else
		void *new_data = av_fast_realloc(data, &size, min_size);
		if (new_data == nullptr)
			return AVERROR(ENOMEM);
		data = new_data;
#endif
		return data;
	}

	template<typename T>
	T *GetT(size_t n) {
		return (T *)Get(n * sizeof(T));
	}
};

#endif
