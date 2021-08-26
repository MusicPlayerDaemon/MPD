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

#include "Filtered.hxx"
#include "Registry.hxx"
#include "Domain.hxx"
#include "OutputAPI.hxx"
#include "Defaults.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "pcm/AudioParser.hxx"
#include "mixer/MixerList.hxx"
#include "mixer/MixerType.hxx"
#include "mixer/MixerControl.hxx"
#include "filter/LoadChain.hxx"
#include "filter/Prepared.hxx"
#include "filter/plugins/AutoConvertFilterPlugin.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "filter/plugins/TwoFilters.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "filter/plugins/NormalizeFilterPlugin.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringAPI.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

#include <cassert>
#include <stdexcept>

#include <string.h>

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"
#define AUDIO_FILTERS		"filters"

FilteredAudioOutput::FilteredAudioOutput(const char *_plugin_name,
					 std::unique_ptr<AudioOutput> &&_output,
					 const ConfigBlock &block,
					 const AudioOutputDefaults &defaults,
					 FilterFactory *filter_factory)
	:plugin_name(_plugin_name), output(std::move(_output))
{
	Configure(block, defaults, filter_factory);
}

static const AudioOutputPlugin *
audio_output_detect()
{
	LogInfo(output_domain, "Attempt to detect audio output device");

	audio_output_plugins_for_each(plugin) {
		if (plugin->test_default_device == nullptr)
			continue;

		FmtInfo(output_domain,
			"Attempting to detect a {} audio device",
			plugin->name);
		if (ao_plugin_test_default_device(plugin))
			return plugin;
	}

	throw std::runtime_error("Unable to detect an audio device");
}

/**
 * Determines the mixer type which should be used for the specified
 * configuration block.
 *
 * This handles the deprecated options mixer_type (global) and
 * mixer_enabled, if the mixer_type setting is not configured.
 */
static MixerType
audio_output_mixer_type(const ConfigBlock &block,
			const AudioOutputDefaults &defaults)
{
	/* read the local "mixer_type" setting */
	const char *p = block.GetBlockValue("mixer_type");
	if (p != nullptr)
		return mixer_type_parse(p);

	/* try the local "mixer_enabled" setting next (deprecated) */
	if (!block.GetBlockValue("mixer_enabled", true))
		return MixerType::NONE;

	/* fall back to the global "mixer_type" setting (also
	   deprecated) */
	return defaults.mixer_type;
}

static Mixer *
audio_output_load_mixer(EventLoop &event_loop, FilteredAudioOutput &ao,
			const ConfigBlock &block,
			const MixerType mixer_type,
			const MixerPlugin *plugin,
			std::unique_ptr<PreparedFilter> &filter_chain,
			MixerListener &listener)
{
	Mixer *mixer;

	switch (mixer_type) {
	case MixerType::NONE:
		return nullptr;

	case MixerType::NULL_:
		return mixer_new(event_loop, null_mixer_plugin,
				 *ao.output, listener,
				 block);

	case MixerType::HARDWARE:
		if (plugin == nullptr)
			return nullptr;

		return mixer_new(event_loop, *plugin,
				 *ao.output, listener,
				 block);

	case MixerType::SOFTWARE:
		mixer = mixer_new(event_loop, software_mixer_plugin,
				  *ao.output, listener,
				  ConfigBlock());
		assert(mixer != nullptr);

		filter_chain = ChainFilters(std::move(filter_chain),
					    ao.volume_filter.Set(volume_filter_prepare()),
					    "software_mixer");
		return mixer;
	}

	assert(false);
	gcc_unreachable();
}

void
FilteredAudioOutput::Configure(const ConfigBlock &block,
			       const AudioOutputDefaults &defaults,
			       FilterFactory *filter_factory)
{
	if (!block.IsNull()) {
		name = block.GetBlockValue(AUDIO_OUTPUT_NAME);
		if (name == nullptr)
			throw std::runtime_error("Missing \"name\" configuration");

		const char *p = block.GetBlockValue(AUDIO_OUTPUT_FORMAT);
		if (p != nullptr)
			config_audio_format = ParseAudioFormat(p, true);
		else
			config_audio_format.Clear();
	} else {
		name = "default detected output";

		config_audio_format.Clear();
	}

	log_name = StringFormat<256>("\"%s\" (%s)", name, plugin_name);

	/* create the normalization filter (if configured) */

	if (defaults.normalize) {
		prepared_filter = ChainFilters(std::move(prepared_filter),
					       autoconvert_filter_new(normalize_filter_prepare()),
					       "normalize");
	}

	try {
		if (filter_factory != nullptr)
			filter_chain_parse(prepared_filter, *filter_factory,
					   block.GetBlockValue(AUDIO_FILTERS, ""));
	} catch (...) {
		/* It's not really fatal - Part of the filter chain
		   has been set up already and even an empty one will
		   work (if only with unexpected behaviour) */
		FmtError(output_domain,
			 "Failed to initialize filter chain for '{}': {}",
			 name, std::current_exception());
	}
}

inline void
FilteredAudioOutput::Setup(EventLoop &event_loop,
			   const ReplayGainConfig &replay_gain_config,
			   const MixerPlugin *mixer_plugin,
			   MixerListener &mixer_listener,
			   const ConfigBlock &block,
			   const AudioOutputDefaults &defaults)
{
	if (output->GetNeedFullyDefinedAudioFormat() &&
	    !config_audio_format.IsFullyDefined())
		throw std::runtime_error("Need full audio format specification");

	const auto mixer_type = audio_output_mixer_type(block, defaults);

	/* create the replay_gain filter */

	const char *replay_gain_handler =
		block.GetBlockValue("replay_gain_handler", "software");

	if (!StringIsEqual(replay_gain_handler, "none")) {
		/* when using software volume, we lose quality by
		   invoking PcmVolume::Apply() twice; to avoid losing
		   too much precision, we allow the ReplayGainFilter
		   to convert 16 bit to 24 bit */
		const bool allow_convert = mixer_type == MixerType::SOFTWARE;

		prepared_replay_gain_filter =
			NewReplayGainFilter(replay_gain_config, allow_convert);
		assert(prepared_replay_gain_filter != nullptr);

		prepared_other_replay_gain_filter =
			NewReplayGainFilter(replay_gain_config, allow_convert);
		assert(prepared_other_replay_gain_filter != nullptr);
	}

	/* set up the mixer */

	try {
		mixer = audio_output_load_mixer(event_loop, *this, block,
						mixer_type,
						mixer_plugin,
						prepared_filter,
						mixer_listener);
	} catch (...) {
		FmtError(output_domain,
			 "Failed to initialize hardware mixer for '{}': {}",
			 name, std::current_exception());
	}

	/* use the hardware mixer for replay gain? */

	if (StringIsEqual(replay_gain_handler, "mixer")) {
		if (mixer != nullptr)
			replay_gain_filter_set_mixer(*prepared_replay_gain_filter,
						     mixer, 100);
		else
			FmtError(output_domain,
				 "No such mixer for output '{}'", name);
	} else if (!StringIsEqual(replay_gain_handler, "software") &&
		   prepared_replay_gain_filter != nullptr) {
		throw std::runtime_error("Invalid \"replay_gain_handler\" value");
	}

	/* the "convert" filter must be the last one in the chain */

	prepared_filter = ChainFilters(std::move(prepared_filter),
				       convert_filter.Set(convert_filter_prepare()),
				       "convert");
}

std::unique_ptr<FilteredAudioOutput>
audio_output_new(EventLoop &normal_event_loop, EventLoop &rt_event_loop,
		 const ReplayGainConfig &replay_gain_config,
		 const ConfigBlock &block,
		 const AudioOutputDefaults &defaults,
		 FilterFactory *filter_factory,
		 MixerListener &mixer_listener)
{
	const AudioOutputPlugin *plugin;

	if (!block.IsNull()) {
		const char *p;

		p = block.GetBlockValue(AUDIO_OUTPUT_TYPE);
		if (p == nullptr)
			throw std::runtime_error("Missing \"type\" configuration");

		plugin = AudioOutputPlugin_get(p);
		if (plugin == nullptr)
			throw FormatRuntimeError("No such audio output plugin: %s", p);
	} else {
		LogWarning(output_domain,
			   "No 'audio_output' defined in config file");

		plugin = audio_output_detect();

		FmtNotice(output_domain,
			  "Successfully detected a {} audio device",
			  plugin->name);
	}

	/* use the real-time I/O thread only for the ALSA plugin;
	   other plugins like httpd don't need real-time so badly,
	   because they have larger buffers */
	// TODO: don't hard-code the plugin name
	auto &event_loop = StringIsEqual(plugin->name, "alsa")
		? rt_event_loop
		: normal_event_loop;

	std::unique_ptr<AudioOutput> ao(ao_plugin_init(event_loop, *plugin,
						       block));
	assert(ao != nullptr);

	auto f = std::make_unique<FilteredAudioOutput>(plugin->name,
						       std::move(ao), block,
						       defaults,
						       filter_factory);
	f->Setup(event_loop, replay_gain_config,
		 plugin->mixer_plugin,
		 mixer_listener, block, defaults);
	return f;
}
