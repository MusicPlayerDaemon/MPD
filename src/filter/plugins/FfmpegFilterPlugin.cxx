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

#include "FfmpegFilterPlugin.hxx"
#include "FfmpegFilter.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "lib/ffmpeg/Filter.hxx"
#include "config/Block.hxx"

class PreparedFfmpegFilter final : public PreparedFilter {
	const char *const graph_string;

public:
	explicit PreparedFfmpegFilter(const char *_graph) noexcept
		:graph_string(_graph) {}

	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedFfmpegFilter::Open(AudioFormat &in_audio_format)
{
	Ffmpeg::FilterGraph graph;

	auto buffer_src =
		Ffmpeg::FilterContext::MakeAudioBufferSource(in_audio_format,
							     *graph);

	auto buffer_sink = Ffmpeg::FilterContext::MakeAudioBufferSink(*graph);

	Ffmpeg::FilterInOut io_sink("out", *buffer_sink);
	Ffmpeg::FilterInOut io_src("in", *buffer_src);
	auto io = graph.Parse(graph_string, std::move(io_sink),
			      std::move(io_src));

	if (io.first.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open input");

	if (io.second.get() != nullptr)
		throw std::runtime_error("FFmpeg filter has an open output");

	graph.CheckAndConfigure();

	auto out_audio_format = in_audio_format; // TODO

	return std::make_unique<FfmpegFilter>(in_audio_format,
					      out_audio_format,
					      std::move(graph),
					      std::move(buffer_src),
					      std::move(buffer_sink));
}

static std::unique_ptr<PreparedFilter>
ffmpeg_filter_init(const ConfigBlock &block)
{
	const char *graph = block.GetBlockValue("graph");
	if (graph == nullptr)
		throw std::runtime_error("Missing \"graph\" configuration");

	/* check if the graph can be parsed (and discard the
	   object) */
	Ffmpeg::FilterGraph().Parse(graph);

	return std::make_unique<PreparedFfmpegFilter>(graph);
}

const FilterPlugin ffmpeg_filter_plugin = {
	"ffmpeg",
	ffmpeg_filter_init,
};
