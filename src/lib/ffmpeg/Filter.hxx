// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_FILTER_HXX
#define MPD_FFMPEG_FILTER_HXX

/* necessary because libavutil/common.h uses UINT64_C */
#define __STDC_CONSTANT_MACROS

#include "Error.hxx"

extern "C" {
#include <libavfilter/avfilter.h>
}

#include <new>
#include <utility>

struct AudioFormat;

namespace Ffmpeg {

class FilterInOut {
	friend class FilterGraph;

	AVFilterInOut *io = nullptr;

	explicit FilterInOut(AVFilterInOut *_io) noexcept
		:io(_io) {}

public:
	FilterInOut() = default;

	FilterInOut(const char *name, AVFilterContext &context)
		:io(avfilter_inout_alloc()) {
		if (io == nullptr)
			throw std::bad_alloc();

		io->name = av_strdup(name);
		io->filter_ctx = &context;
		io->pad_idx = 0;
		io->next = nullptr;
	}

	FilterInOut(FilterInOut &&src) noexcept
		:io(std::exchange(src.io, nullptr)) {}

	~FilterInOut() noexcept {
		if (io != nullptr)
			avfilter_inout_free(&io);
	}

	FilterInOut &operator=(FilterInOut &&src) noexcept {
		using std::swap;
		swap(io, src.io);
		return *this;
	}

	auto *get() noexcept {
		return io;
	}
};

/**
 * Create an "abuffer" filter.
 *
 * @param the input audio format; may be modified by the
 * function to ask the caller to do format conversion
 */
AVFilterContext &
MakeAudioBufferSource(AudioFormat &audio_format,
		      AVFilterGraph &graph_ctx);

/**
 * Create an "abuffersink" filter.
 */
AVFilterContext &
MakeAudioBufferSink(AVFilterGraph &graph_ctx);

/**
 * Create an "aformat" filter.
 *
 * @param the output audio format; may be modified by the function if
 * the given format is not supported by libavfilter
 */
AVFilterContext &
MakeAformat(AudioFormat &audio_format,
	    AVFilterGraph &graph_ctx);

/**
 * Create an "aformat" filter which automatically converts the output
 * to a format supported by MPD.
 */
AVFilterContext &
MakeAutoAformat(AVFilterGraph &graph_ctx);

class FilterGraph {
	AVFilterGraph *graph = nullptr;

public:
	FilterGraph(std::nullptr_t) noexcept {}

	FilterGraph():graph(avfilter_graph_alloc()) {
		if (graph == nullptr)
			throw std::bad_alloc();
	}

	FilterGraph(FilterGraph &&src) noexcept
		:graph(std::exchange(src.graph, nullptr)) {}

	~FilterGraph() noexcept {
		if (graph != nullptr)
			avfilter_graph_free(&graph);
	}

	FilterGraph &operator=(FilterGraph &&src) noexcept {
		using std::swap;
		swap(graph, src.graph);
		return *this;
	}

	auto &operator*() noexcept {
		return *graph;
	}

	auto *operator->() noexcept {
		return graph;
	}

	std::pair<FilterInOut, FilterInOut> Parse(const char *filters,
						  FilterInOut &&inputs,
						  FilterInOut &&outputs) {
		int err = avfilter_graph_parse_ptr(graph, filters,
						   &inputs.io, &outputs.io,
						   nullptr);
		if (err < 0)
			throw MakeFfmpegError(err, "avfilter_graph_parse_ptr() failed");

		return {std::move(inputs), std::move(outputs)};
	}

	void ParseSingleInOut(const char *filters, AVFilterContext &in,
			      AVFilterContext &out);

	std::pair<FilterInOut, FilterInOut> Parse(const char *filters) {
		AVFilterInOut *inputs, *outputs;
		int err = avfilter_graph_parse2(graph, filters,
						&inputs, &outputs);
		if (err < 0)
			throw MakeFfmpegError(err, "avfilter_graph_parse2() failed");

		return {FilterInOut{inputs}, FilterInOut{outputs}};
	}

	void CheckAndConfigure() {
		int err = avfilter_graph_config(graph, nullptr);
		if (err < 0)
			throw MakeFfmpegError(err, "avfilter_graph_config() failed");
	}
};

}

#endif
