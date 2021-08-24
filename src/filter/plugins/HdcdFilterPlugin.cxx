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

#include "HdcdFilterPlugin.hxx"
#include "FfmpegFilter.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"
#include "lib/ffmpeg/Filter.hxx"
#include "pcm/AudioFormat.hxx"

static constexpr const char *hdcd_graph = "hdcd";

gcc_pure
static bool
MaybeHdcd(const AudioFormat &audio_format) noexcept
{
	return audio_format.sample_rate == 44100 &&
		audio_format.format == SampleFormat::S16 &&
		audio_format.channels == 2;
}

static auto
OpenHdcdFilter(AudioFormat &in_audio_format)
{
	Ffmpeg::FilterGraph graph;

	auto &buffer_src =
		Ffmpeg::MakeAudioBufferSource(in_audio_format,
					      *graph);

	auto &buffer_sink = Ffmpeg::MakeAudioBufferSink(*graph);

	graph.ParseSingleInOut(hdcd_graph, buffer_sink, buffer_src);
	graph.CheckAndConfigure();

	auto out_audio_format = in_audio_format;
	// TODO: convert to 32 bit only if HDCD actually detected
	out_audio_format.format = SampleFormat::S32;

	return std::make_unique<FfmpegFilter>(in_audio_format,
					      out_audio_format,
					      std::move(graph),
					      buffer_src,
					      buffer_sink);
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
		return OpenHdcdFilter(audio_format);
	else
		/* this cannot be HDCD, so let's copy as-is using
		   NullFilter */
		return std::make_unique<NullFilter>(audio_format);
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
