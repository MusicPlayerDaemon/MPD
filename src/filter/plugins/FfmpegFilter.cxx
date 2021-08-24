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

#include "FfmpegFilter.hxx"
#include "lib/ffmpeg/Interleave.hxx"
#include "lib/ffmpeg/SampleFormat.hxx"
#include "util/ConstBuffer.hxx"

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
	 in_channels(in_audio_format.channels),
	 in_audio_frame_size(in_audio_format.GetFrameSize()),
	 out_audio_frame_size(_out_audio_format.GetFrameSize())
{
}

ConstBuffer<void>
FfmpegFilter::FilterPCM(ConstBuffer<void> src)
{
	/* submit source data into the FFmpeg audio buffer source */

	frame.Unref();
	frame->format = in_format;
	frame->sample_rate = in_sample_rate;
	frame->channels = in_channels;
	frame->nb_samples = src.size / in_audio_frame_size;

	frame.GetBuffer();

	memcpy(frame.GetData(0), src.data, src.size);

	int err = av_buffersrc_add_frame(&buffer_src, frame.get());
	if (err < 0)
		throw MakeFfmpegError(err, "av_buffersrc_write_frame() failed");

	/* collect filtered data from the FFmpeg audio buffer sink */

	frame.Unref();

	err = av_buffersink_get_frame(&buffer_sink, frame.get());
	if (err < 0) {
		if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
			return nullptr;

		throw MakeFfmpegError(err, "av_buffersink_get_frame() failed");
	}

	/* TODO: call av_buffersink_get_frame() repeatedly?  Not
	   possible with MPD's current Filter API */

	return Ffmpeg::InterleaveFrame(*frame, interleave_buffer);
}
