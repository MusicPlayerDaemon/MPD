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

#include "HdcdFilterPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"
#include "lib/ffmpeg/Filter.hxx"
#include "lib/ffmpeg/Frame.hxx"
#include "lib/ffmpeg/SampleFormat.hxx"
#include "config/Block.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"

extern "C" {
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include <string.h>

static constexpr const char *hdcd_graph = "hdcd";

gcc_pure
static bool
MaybeHdcd(const AudioFormat &audio_format) noexcept
{
	return audio_format.sample_rate == 44100 &&
		audio_format.format == SampleFormat::S16 &&
		audio_format.channels == 2;
}

class HdcdFilter final : public Filter {
	Ffmpeg::FilterGraph graph;
	Ffmpeg::FilterContext buffer_src, buffer_sink;
	Ffmpeg::Frame frame, out_frame;

	size_t in_audio_frame_size;
	size_t out_audio_frame_size;

public:
	explicit HdcdFilter(AudioFormat &audio_format);

	/* virtual methods from class Filter */
	ConstBuffer<void> FilterPCM(ConstBuffer<void> src) override;
};

inline
HdcdFilter::HdcdFilter(AudioFormat &audio_format)
	:Filter(audio_format)
{
	buffer_src = Ffmpeg::FilterContext::MakeAudioBufferSource(audio_format,
								  *graph);

	buffer_sink = Ffmpeg::FilterContext::MakeAudioBufferSink(*graph);

	Ffmpeg::FilterInOut io_sink("out", *buffer_sink);
	Ffmpeg::FilterInOut io_src("in", *buffer_src);
	auto io = graph.Parse(hdcd_graph, std::move(io_sink),
			      std::move(io_src));

	if (io.first.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open input");

	if (io.second.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open output");

	graph.CheckAndConfigure();

	frame->format = Ffmpeg::ToFfmpegSampleFormat(audio_format.format);
	frame->sample_rate = audio_format.sample_rate;
	frame->channels = audio_format.channels;

	// TODO: convert to 32 bit only if HDCD actually detected
	out_audio_format.format = SampleFormat::S32;

	in_audio_frame_size = audio_format.GetFrameSize();
	out_audio_frame_size = audio_format.GetFrameSize();
}

class PreparedHdcdFilter final : public PreparedFilter {
public:
	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedHdcdFilter::Open(AudioFormat &audio_format)
{
	if (MaybeHdcd(audio_format))
		return std::make_unique<HdcdFilter>(audio_format);
	else
		/* this cannot be HDCD, so let's copy as-is using
		   NullFilter */
		return std::make_unique<NullFilter>(audio_format);
}

ConstBuffer<void>
HdcdFilter::FilterPCM(ConstBuffer<void> src)
{
	/* submit source data into the FFmpeg audio buffer source */

	frame->nb_samples = src.size / in_audio_frame_size;

	frame.GetBuffer();
	frame.MakeWritable();

	memcpy(frame.GetData(0), src.data, src.size);

	int err = av_buffersrc_write_frame(buffer_src.get(), frame.get());
	if (err < 0)
		throw MakeFfmpegError(err, "av_buffersrc_write_frame() failed");

	/* collect filtered data from the FFmpeg audio buffer sink */

	err = av_buffersink_get_frame(buffer_sink.get(), out_frame.get());
	if (err < 0) {
		if (err == AVERROR(EAGAIN) || err == AVERROR_EOF)
			return nullptr;

		throw MakeFfmpegError(err, "av_buffersink_get_frame() failed");
	}

	/* TODO: call av_buffersink_get_frame() repeatedly?  Not
	   possible with MPD's current Filter API */

	return {out_frame.GetData(0), out_frame->nb_samples * GetOutAudioFormat().GetFrameSize()};
}

static std::unique_ptr<PreparedFilter>
hdcd_filter_init(const ConfigBlock &)
{
	/* check if the graph can be parsed (and discard the
	   object) */
	Ffmpeg::FilterGraph().Parse(hdcd_graph);

	return std::make_unique<PreparedHdcdFilter>();
}

const FilterPlugin hdcd_filter_plugin = {
	"hdcd",
	hdcd_filter_init,
};
