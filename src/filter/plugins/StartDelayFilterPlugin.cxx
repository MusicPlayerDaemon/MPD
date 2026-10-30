// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "StartDelayFilterPlugin.hxx"
#include "StartDelayFilter.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/NullFilter.hxx"
#include "filter/Prepared.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"

static constexpr unsigned STARTDELAY_DEFAULT_MS = 1000;

/** amplitude of the noise, in LSBs of a 16 bit sample */
static constexpr unsigned STARTDELAY_NOISE_LEVEL_DEFAULT = 0;
static constexpr unsigned STARTDELAY_NOISE_LEVEL_MAX = 255;

class PreparedStartDelayFilter final : public PreparedFilter {
	std::chrono::milliseconds time;
	unsigned level;
	bool on_seek;

	bool FormatSupported(const AudioFormat &audio_format) const noexcept {
		return audio_format.format != SampleFormat::DSD &&
			 audio_format.format != SampleFormat::UNDEFINED;
	}

public:
	explicit PreparedStartDelayFilter(std::chrono::milliseconds _time, unsigned int _level, bool _on_seek) noexcept
		:time(_time),
		 level(_level),
		 on_seek(_on_seek)
		{}

	/* virtual methods from class PreparedFilter */
	std::unique_ptr<Filter> Open(AudioFormat &af) override;
};

std::unique_ptr<Filter>
PreparedStartDelayFilter::Open(AudioFormat &audio_format)
{
	if ((FormatSupported(audio_format) || level == 0) && time.count() > 0)
		return std::make_unique<StartDelayFilter>(audio_format,
					      time,
					      level,
					      on_seek);
	else
		/* Unable to generate noise in the required format,
		   so let's copy as-is using NullFilter */
		return std::make_unique<NullFilter>(audio_format);
}

static std::unique_ptr<PreparedFilter>
startdelay_filter_init(const ConfigBlock &block)
{
	auto time = block.GetBlockValue("delay_ms", STARTDELAY_DEFAULT_MS);
	auto level = block.GetBlockValue("noise_level", STARTDELAY_NOISE_LEVEL_DEFAULT);
	if (level > STARTDELAY_NOISE_LEVEL_MAX)
		throw FmtRuntimeError("noise_level ({}) out of range [0 - {}]", level, STARTDELAY_NOISE_LEVEL_MAX);
	auto on_seek = block.GetBlockValue("on_seek", false);

	return std::make_unique<PreparedStartDelayFilter>(std::chrono::milliseconds(time), level, on_seek);
}

const FilterPlugin startdelay_filter_plugin = {
	"startdelay",
	startdelay_filter_init,
};
