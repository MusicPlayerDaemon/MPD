// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StartDelayFilterPlugin.hxx"
#include "StartDelayFilter.hxx"
#include "config/Block.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "pcm/AudioFormat.hxx"

#include <chrono>
#include <memory>

static constexpr unsigned STARTDELAY_DEFAULT_MS = 1000;

/** amplitude of the noise, in LSBs of a 16 bit sample */
static constexpr unsigned STARTDELAY_NOISE_LEVEL_DEFAULT = 0;
static constexpr unsigned STARTDELAY_NOISE_LEVEL_MAX = 255;

class PreparedStartDelayFilter final : public PreparedFilter {
	const std::chrono::milliseconds time;
	const unsigned level;
	const bool on_reset;

public:
	explicit PreparedStartDelayFilter(std::chrono::milliseconds _time,
				unsigned int _level,
				bool _on_reset) noexcept
		:time(_time),
		 level(_level),
		 on_reset(_on_reset)
		{}

	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedStartDelayFilter::Open(AudioFormat &audio_format)
{
	auto local_level = level;
	if (audio_format.format == SampleFormat::DSD)
		local_level = 0;

	if (time.count() > 0)
		return std::make_unique<StartDelayFilter>(
					audio_format,
					time,
					local_level,
					on_reset);
	else
		/* When the delay is zero there is no work to do,
		   so let's copy as-is using NullFilter */
		return std::make_unique<NullFilter>(audio_format);
}

static std::unique_ptr<PreparedFilter>
startdelay_filter_init(const ConfigBlock &block)
{
	const auto time = block.GetBlockValue("delay", STARTDELAY_DEFAULT_MS);
	const auto level = block.GetBlockValue("noise_level", STARTDELAY_NOISE_LEVEL_DEFAULT);
	if (level > STARTDELAY_NOISE_LEVEL_MAX)
		throw FmtRuntimeError("noise_level ({}) out of range [0 - {}]", level, STARTDELAY_NOISE_LEVEL_MAX);
	const auto on_reset = block.GetBlockValue("on_reset", false);

	return std::make_unique<PreparedStartDelayFilter>(std::chrono::milliseconds(time), level, on_reset);
}

const FilterPlugin startdelay_filter_plugin = {
	"startdelay",
	startdelay_filter_init,
};
