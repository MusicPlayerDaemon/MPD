// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Interleave.hxx"
#include "Buffer.hxx"
#include "Error.hxx"
#include "pcm/Interleave.hxx"

extern "C" {
#include <libavutil/frame.h>
}

#include <cassert>
#include <new> // for std::bad_alloc

namespace Ffmpeg {

std::span<const std::byte>
InterleaveFrame(const AVFrame &frame, FfmpegBuffer &buffer)
{
	assert(frame.nb_samples > 0);

	const auto format = AVSampleFormat(frame.format);
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 25, 100)
	const unsigned channels = frame.ch_layout.nb_channels;
#else
	const unsigned channels = frame.channels;
#endif
	const std::size_t n_frames = frame.nb_samples;

	int plane_size;
	const int data_size =
		av_samples_get_buffer_size(&plane_size, channels,
					   n_frames, format, 1);
	assert(data_size != 0);
	if (data_size < 0)
		throw MakeFfmpegError(data_size);

	std::byte *output_buffer;
	if (av_sample_fmt_is_planar(format) && channels > 1) {
		output_buffer = buffer.GetT<std::byte>(data_size);
		if (output_buffer == nullptr)
			/* Not enough memory - shouldn't happen */
			throw std::bad_alloc();

		PcmInterleave(output_buffer,
			      {(const void *const*)frame.extended_data, channels},
			      n_frames,
			      av_get_bytes_per_sample(format));
	} else {
		output_buffer = (std::byte *)frame.extended_data[0];
	}

	return { output_buffer, (size_t)data_size };
}

} // namespace Ffmpeg
