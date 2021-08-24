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

	const int in_format, in_sample_rate, in_channels;

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
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
};

#endif
