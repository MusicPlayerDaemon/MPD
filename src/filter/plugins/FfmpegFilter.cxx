// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FfmpegFilter.hxx"
#include "lib/ffmpeg/Interleave.hxx"
#include "lib/ffmpeg/SampleFormat.hxx"

extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include <string.h>

FfmpegFilter::FfmpegFilter(const AudioFormat &in_audio_format,
			   const AudioFormat &_out_audio_format,
			   Ffmpeg::FilterGraph &&_graph,
			   AVFilterContext &_buffer_src,
			   AVFilterContext &_buffer_sink) noexcept
	:Filter(_out_audio_format),
	 graph(std::move(_graph)),
	 buffer_src(_buffer_src),
	 buffer_sink(_buffer_sink),
	 in_format(Ffmpeg::ToFfmpegSampleFormat(in_audio_format.format)),
	 in_sample_rate(in_audio_format.sample_rate),
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 25, 100)
	 in_channels(in_audio_format.channels),
#endif
	 in_audio_frame_size(in_audio_format.GetFrameSize()),
	 out_audio_frame_size(_out_audio_format.GetFrameSize())
{
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 25, 100)
	av_channel_layout_default(&in_ch_layout, in_audio_format.channels);
#endif
}

std::span<const std::byte>
FfmpegFilter::FilterPCM(std::span<const std::byte> src)
{
	/* submit source data into the FFmpeg audio buffer source */

	frame.Unref();
	frame->format = in_format;
	frame->sample_rate = in_sample_rate;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(57, 25, 100)
	frame->ch_layout = in_ch_layout;
#else
	frame->channels = in_channels;
#endif
	frame->nb_samples = src.size() / in_audio_frame_size;

	frame.GetBuffer();

	memcpy(frame.GetData(0), src.data(), src.size());

	int err = av_buffersrc_add_frame(&buffer_src, frame.get());
	if (err < 0)
		throw MakeFfmpegError(err, "av_buffersrc_write_frame() failed");

	/* collect filtered data from the FFmpeg audio buffer sink */

	frame.Unref();

	err = av_buffersink_get_frame(&buffer_sink, frame.get());
	if (err < 0) {
		if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
			return {};

		throw MakeFfmpegError(err, "av_buffersink_get_frame() failed");
	}

	/* TODO: call av_buffersink_get_frame() repeatedly?  Not
	   possible with MPD's current Filter API */

	return Ffmpeg::InterleaveFrame(*frame, interleave_buffer);
}
