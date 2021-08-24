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

#include "DetectFilterFormat.hxx"
#include "Frame.hxx"
#include "SampleFormat.hxx"
#include "pcm/Silence.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "util/WritableBuffer.hxx"

extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include <cassert>

namespace Ffmpeg {

AudioFormat
DetectFilterOutputFormat(const AudioFormat &in_audio_format,
			 AVFilterContext &buffer_src,
			 AVFilterContext &buffer_sink)
{
	uint_least64_t silence[MAX_CHANNELS];
	const size_t silence_size = in_audio_format.GetFrameSize();
	assert(sizeof(silence) >= silence_size);

	PcmSilence(WritableBuffer<void>{&silence, silence_size},
		   in_audio_format.format);

	Frame frame;
	frame->format = ToFfmpegSampleFormat(in_audio_format.format);
	frame->sample_rate = in_audio_format.sample_rate;
	frame->channels = in_audio_format.channels;
	frame->nb_samples = 1;

	frame.GetBuffer();

	memcpy(frame.GetData(0), silence, silence_size);

	int err = av_buffersrc_add_frame(&buffer_src, frame.get());
	if (err < 0)
		throw MakeFfmpegError(err, "av_buffersrc_add_frame() failed");

	frame.Unref();

	err = av_buffersink_get_frame(&buffer_sink, frame.get());
	if (err < 0) {
		if (err == AVERROR(EAGAIN))
			/* one sample was not enough input data for
			   the given filter graph */
			return AudioFormat::Undefined();

		throw MakeFfmpegError(err, "av_buffersink_get_frame() failed");
	}

	const SampleFormat sample_format = FromFfmpegSampleFormat(AVSampleFormat(frame->format));
	if (sample_format == SampleFormat::UNDEFINED)
		throw std::runtime_error("Unsupported FFmpeg sample format");

	return CheckAudioFormat(frame->sample_rate, sample_format,
				frame->channels);
}

} // namespace Ffmpeg
