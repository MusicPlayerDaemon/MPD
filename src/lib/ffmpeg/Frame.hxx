// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_FRAME_HXX
#define MPD_FFMPEG_FRAME_HXX

#include "Error.hxx"

extern "C" {
#include <libavutil/frame.h>
}

#include <new>

namespace Ffmpeg {

class Frame {
	AVFrame *frame;

public:
	Frame():frame(av_frame_alloc()) {
		if (frame == nullptr)
			throw std::bad_alloc();
	}

	~Frame() noexcept {
		av_frame_free(&frame);
	}

	Frame(const Frame &) = delete;
	Frame &operator=(const Frame &) = delete;

	AVFrame &operator*() noexcept {
		return *frame;
	}

	AVFrame *operator->() noexcept {
		return frame;
	}

	AVFrame *get() noexcept {
		return frame;
	}

	void Unref() noexcept {
		av_frame_unref(frame);
	}

	void GetBuffer() {
		int err = av_frame_get_buffer(frame, 0);
		if (err < 0)
			throw MakeFfmpegError(err, "av_frame_get_buffer() failed");
	}

	void MakeWritable() {
		int err = av_frame_make_writable(frame);
		if (err < 0)
			throw MakeFfmpegError(err, "av_frame_make_writable() failed");
	}

	void *GetData(unsigned plane) noexcept {
		return frame->data[plane];
	}
};

} // namespace Ffmpeg

#endif
