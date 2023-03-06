// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_FILTER__HXX
#define MPD_FFMPEG_FILTER__HXX

#include "filter/Filter.hxx"
#include "lib/ffmpeg/Buffer.hxx"
#include "lib/ffmpeg/Filter.hxx"
#include "lib/ffmpeg/Frame.hxx"

/**
 * A #Filter implementation using FFmpeg's libavfilter.
 */
class FfmpegFilter final : public Filter {
	Ffmpeg::FilterGraph graph;
	AVFilterContext &buffer_src, &buffer_sink;
	Ffmpeg::Frame frame;

	FfmpegBuffer interleave_buffer;

	const int in_format, in_sample_rate;

#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 25, 100)
	AVChannelLayout in_ch_layout;
#else
	const int in_channels;
#endif

	const size_t in_audio_frame_size;
	const size_t out_audio_frame_size;

public:
	/**
	 * @param _graph a checked and configured AVFilterGraph
	 * @param _buffer_src an "abuffer" filter which serves as
	 * input
	 * @param _buffer_sink an "abuffersink" filter which serves as
	 * output
	 */
	FfmpegFilter(const AudioFormat &in_audio_format,
		     const AudioFormat &_out_audio_format,
		     Ffmpeg::FilterGraph &&_graph,
		     AVFilterContext &_buffer_src,
		     AVFilterContext &_buffer_sink) noexcept;

	/* virtual methods from class Filter */
	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;
};

#endif
