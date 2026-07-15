// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "AllOutputs.hxx"
#include "Control.hxx"
#include "Defaults.hxx"
#include "Filtered.hxx"
#include "filter/Factory.hxx"
#include "config/Block.hxx"
#include "config/Data.hxx"
#include "config/Option.hxx"
#include "lib/fmt/RuntimeError.hxx"

AllOutputs::AllOutputs() noexcept = default;

AllOutputs::~AllOutputs() noexcept
{
	/* parallel destruction */
	for (const auto &i : outputs)
		i->BeginDestroy();
}

static std::unique_ptr<FilteredAudioOutput>
LoadOutput(EventLoop &event_loop, EventLoop &rt_event_loop,
	   const ReplayGainConfig &replay_gain_config,
	   const ConfigBlock &block,
	   const AudioOutputDefaults &defaults,
	   FilterFactory *filter_factory)
try {
	return audio_output_new(event_loop, rt_event_loop, replay_gain_config, block,
				defaults,
				filter_factory);
} catch (...) {
	if (block.line > 0)
		std::throw_with_nested(FmtRuntimeError("Failed to configure output in line {}",
						       block.line));
	else
		throw;
}

static std::unique_ptr<AudioOutputControl>
LoadOutputControl(EventLoop &event_loop, EventLoop &rt_event_loop,
		  const ReplayGainConfig &replay_gain_config,
		  const ConfigBlock &block,
		  const AudioOutputDefaults &defaults,
		  FilterFactory *filter_factory)
{
	auto output = LoadOutput(event_loop, rt_event_loop,
				 replay_gain_config,
				 block, defaults, filter_factory);
	return std::make_unique<AudioOutputControl>(std::move(output),
						    block);
}

void
AllOutputs::Configure(EventLoop &event_loop, EventLoop &rt_event_loop,
		      const ConfigData &config,
		      const ReplayGainConfig &replay_gain_config)
{
	const AudioOutputDefaults defaults(config);
	FilterFactory filter_factory(config);

	config.WithEach(ConfigBlockOption::AUDIO_OUTPUT, [&, this](const auto &block){
		auto output = LoadOutputControl(event_loop, rt_event_loop,
						replay_gain_config,
						block, defaults,
						&filter_factory);
		if (HasName(output->GetName()))
			throw FmtRuntimeError("output devices with identical "
					      "names: {}",
					      output->GetName());

		outputs.emplace_back(std::move(output));
	});

	if (outputs.empty()) {
		/* auto-detect device */
		const ConfigBlock empty;
		outputs.emplace_back(LoadOutputControl(event_loop,
						       rt_event_loop,
						       replay_gain_config,
						       empty, defaults,
						       nullptr));
	}
}

AudioOutputControl *
AllOutputs::FindByName(const std::string_view name) noexcept
{
	for (const auto &i : outputs)
		if (name == i->GetName())
			return i.get();

	return nullptr;
}
