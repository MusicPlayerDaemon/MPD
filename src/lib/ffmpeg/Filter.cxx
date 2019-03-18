/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "Filter.hxx"
#include "SampleFormat.hxx"
#include "AudioFormat.hxx"
#include "util/RuntimeError.hxx"

#include <stdio.h>

namespace Ffmpeg {

static const auto &
RequireFilterByName(const char *name)
{
	const auto *filter = avfilter_get_by_name(name);
	if (filter == nullptr)
		throw FormatRuntimeError("No such FFmpeg filter: '%s'", name);

	return *filter;
}

FilterContext
FilterContext::MakeAudioBufferSource(AudioFormat &audio_format,
				     AVFilterGraph &graph_ctx)
{
	AVSampleFormat src_format = ToFfmpegSampleFormat(audio_format.format);
	if (src_format == AV_SAMPLE_FMT_NONE) {
		switch (audio_format.format) {
		case SampleFormat::S24_P32:
			audio_format.format = SampleFormat::S32;
			src_format = AV_SAMPLE_FMT_S32;
			break;

		default:
			audio_format.format = SampleFormat::S16;
			src_format = AV_SAMPLE_FMT_S16;
			break;
		}
	}

	char abuffer_args[256];
	sprintf(abuffer_args,
		"sample_rate=%u:sample_fmt=%s:channels=%u:time_base=1/%u",
		audio_format.sample_rate,
		av_get_sample_fmt_name(src_format),
		audio_format.channels,
		audio_format.sample_rate);

	return {RequireFilterByName("abuffer"), "abuffer", abuffer_args, nullptr, graph_ctx};
}

FilterContext
FilterContext::MakeAudioBufferSink(AVFilterGraph &graph_ctx)
{
	return {RequireFilterByName("abuffersink"), "abuffersink", graph_ctx};
}

} // namespace Ffmpeg
