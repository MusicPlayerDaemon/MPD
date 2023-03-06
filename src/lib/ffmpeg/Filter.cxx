// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Filter.hxx"
#include "ChannelLayout.hxx"
#include "SampleFormat.hxx"
#include "pcm/AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/ToBuffer.hxx"

#include <stdio.h>

namespace Ffmpeg {

static const auto &
RequireFilterByName(const char *name)
{
	const auto *filter = avfilter_get_by_name(name);
	if (filter == nullptr)
		throw FmtRuntimeError("No such FFmpeg filter: '{}'", name);

	return *filter;
}

static AVFilterContext &
CreateFilter(const AVFilter &filt,
	     const char *name, const char *args, void *opaque,
	     AVFilterGraph &graph_ctx)
{
	AVFilterContext *context = nullptr;
	int err = avfilter_graph_create_filter(&context, &filt,
					       name, args, opaque,
					       &graph_ctx);
	if (err < 0)
		throw MakeFfmpegError(err, "avfilter_graph_create_filter() failed");

	return *context;
}

static AVFilterContext &
CreateFilter(const AVFilter &filt,
	     const char *name,
	     AVFilterGraph &graph_ctx)
{
	return CreateFilter(filt, name, nullptr, nullptr, graph_ctx);
}

AVFilterContext &
MakeAudioBufferSource(AudioFormat &audio_format,
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

	const auto abuffer_args = FmtBuffer<256>(
		"sample_rate={}:sample_fmt={}:channel_layout={:#x}:time_base=1/{}",
		audio_format.sample_rate,
		av_get_sample_fmt_name(src_format),
		ToFfmpegChannelLayout(audio_format.channels),
		audio_format.sample_rate);

	return CreateFilter(RequireFilterByName("abuffer"), "abuffer",
			    abuffer_args, nullptr, graph_ctx);
}

AVFilterContext &
MakeAudioBufferSink(AVFilterGraph &graph_ctx)
{
	return CreateFilter(RequireFilterByName("abuffersink"), "abuffersink",
			    graph_ctx);
}

AVFilterContext &
MakeAformat(AudioFormat &audio_format,
	    AVFilterGraph &graph_ctx)
{
	AVSampleFormat dest_format = ToFfmpegSampleFormat(audio_format.format);
	if (dest_format == AV_SAMPLE_FMT_NONE) {
		switch (audio_format.format) {
		case SampleFormat::S24_P32:
			audio_format.format = SampleFormat::S32;
			dest_format = AV_SAMPLE_FMT_S32;
			break;

		default:
			audio_format.format = SampleFormat::S16;
			dest_format = AV_SAMPLE_FMT_S16;
			break;
		}
	}

	const auto args = FmtBuffer<256>(
		"sample_rates={}:sample_fmts={}:channel_layouts={:#x}",
		audio_format.sample_rate,
		av_get_sample_fmt_name(dest_format),
		ToFfmpegChannelLayout(audio_format.channels));

	return CreateFilter(RequireFilterByName("aformat"), "aformat",
			    args, nullptr, graph_ctx);
}

AVFilterContext &
MakeAutoAformat(AVFilterGraph &graph_ctx)
{
	return CreateFilter(RequireFilterByName("aformat"), "aformat",
			    "sample_fmts=flt|s32|s16",
			    nullptr, graph_ctx);
}

void
FilterGraph::ParseSingleInOut(const char *filters, AVFilterContext &in,
			      AVFilterContext &out)
{
	auto [inputs, outputs] = Parse(filters, {"out", in}, {"in", out});

	if (inputs.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open input");

	if (outputs.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open output");
}

} // namespace Ffmpeg
