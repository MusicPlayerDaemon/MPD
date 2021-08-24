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

#include "FfmpegFilterPlugin.hxx"
#include "FfmpegFilter.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/Filter.hxx"
#include "filter/Prepared.hxx"
#include "lib/ffmpeg/Filter.hxx"
#include "lib/ffmpeg/DetectFilterFormat.hxx"
#include "config/Block.hxx"

class PreparedFfmpegFilter final : public PreparedFilter {
	const char *const graph_string;

public:
	explicit PreparedFfmpegFilter(const char *_graph) noexcept
		:graph_string(_graph) {}

	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

/**
 * Fallback for PreparedFfmpegFilter::Open() just in case the filter's
 * native output format could not be determined.
 *
 * TODO: improve the MPD filter API to allow returning the output
 * format later, and eliminate this kludge
 */
static auto
OpenWithAformat(const char *graph_string, AudioFormat &in_audio_format)
{
	Ffmpeg::FilterGraph graph;

	auto &buffer_src =
		Ffmpeg::MakeAudioBufferSource(in_audio_format, *graph);

	auto &buffer_sink = Ffmpeg::MakeAudioBufferSink(*graph);

	AudioFormat out_audio_format = in_audio_format;
	auto &aformat = Ffmpeg::MakeAformat(out_audio_format, *graph);

	int error = avfilter_link(&aformat, 0, &buffer_sink, 0);
	if (error < 0)
		throw MakeFfmpegError(error, "avfilter_link() failed");

	graph.ParseSingleInOut(graph_string, aformat, buffer_src);
	graph.CheckAndConfigure();

	return std::make_unique<FfmpegFilter>(in_audio_format,
					      out_audio_format,
					      std::move(graph),
					      buffer_src,
					      buffer_sink);
}

std::unique_ptr<Filter>
PreparedFfmpegFilter::Open(AudioFormat &in_audio_format)
{
	Ffmpeg::FilterGraph graph;

	auto &buffer_src =
		Ffmpeg::MakeAudioBufferSource(in_audio_format, *graph);

	auto &buffer_sink = Ffmpeg::MakeAudioBufferSink(*graph);

	/* if the filter's output format is not supported by MPD, this
	   "aformat" filter is inserted at the end and takes care for
	   the required conversion */
	auto &aformat = Ffmpeg::MakeAutoAformat(*graph);

	int error = avfilter_link(&aformat, 0, &buffer_sink, 0);
	if (error < 0)
		throw MakeFfmpegError(error, "avfilter_link() failed");

	graph.ParseSingleInOut(graph_string, aformat, buffer_src);
	graph.CheckAndConfigure();

	const auto out_audio_format =
		Ffmpeg::DetectFilterOutputFormat(in_audio_format, buffer_src,
						 buffer_sink);

	if (!out_audio_format.IsDefined())
		/* the filter's native output format could not be
		   determined yet, but we need to know it now; as a
		   workaround for this MPD API deficiency, try again
		   with an "aformat" filter which forces a specific
		   output format */
		return OpenWithAformat(graph_string, in_audio_format);

	return std::make_unique<FfmpegFilter>(in_audio_format,
					      out_audio_format,
					      std::move(graph),
					      buffer_src,
					      buffer_sink);
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
