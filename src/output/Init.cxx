/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "Filtered.hxx"
#include "Registry.hxx"
#include "Domain.hxx"
#include "OutputAPI.hxx"
#include "filter/FilterConfig.hxx"
#include "AudioParser.hxx"
#include "mixer/MixerList.hxx"
#include "mixer/MixerType.hxx"
#include "mixer/MixerControl.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "filter/plugins/AutoConvertFilterPlugin.hxx"
#include "filter/plugins/ConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "filter/plugins/ChainFilterPlugin.hxx"
#include "filter/plugins/VolumeFilterPlugin.hxx"
#include "filter/plugins/NormalizeFilterPlugin.hxx"
#include "config/ConfigError.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/Block.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <stdexcept>

#include <assert.h>
#include <string.h>

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"
#define AUDIO_FILTERS		"filters"

FilteredAudioOutput::FilteredAudioOutput(AudioOutput &_output,
					 const ConfigBlock &block)
	:output(&_output)
{
#ifndef NDEBUG
	const auto &plugin = output->GetPlugin();
	assert(plugin.finish != nullptr);
	assert(plugin.open != nullptr);
	assert(plugin.close != nullptr);
	assert(plugin.play != nullptr);
#endif

	Configure(block);
}

static const AudioOutputPlugin *
audio_output_detect()
{
	LogDefault(output_domain, "Attempt to detect audio output device");

	audio_output_plugins_for_each(plugin) {
		if (plugin->test_default_device == nullptr)
			continue;

		FormatDefault(output_domain,
			      "Attempting to detect a %s audio device",
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
gcc_pure
static MixerType
audio_output_mixer_type(const ConfigBlock &block) noexcept
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
	return mixer_type_parse(config_get_string(ConfigOption::MIXER_TYPE,
						  "hardware"));
}

static Mixer *
audio_output_load_mixer(EventLoop &event_loop, FilteredAudioOutput &ao,
			const ConfigBlock &block,
			const MixerPlugin *plugin,
			PreparedFilter &filter_chain,
			MixerListener &listener)
{
	Mixer *mixer;

	switch (audio_output_mixer_type(block)) {
	case MixerType::NONE:
	case MixerType::UNKNOWN:
		return nullptr;

	case MixerType::NULL_:
		return mixer_new(event_loop, null_mixer_plugin, ao, listener,
				 block);

	case MixerType::HARDWARE:
		if (plugin == nullptr)
			return nullptr;

		return mixer_new(event_loop, *plugin, ao, listener,
				 block);

	case MixerType::SOFTWARE:
		mixer = mixer_new(event_loop, software_mixer_plugin, ao,
				  listener,
				  ConfigBlock());
		assert(mixer != nullptr);

		filter_chain_append(filter_chain, "software_mixer",
				    ao.volume_filter.Set(volume_filter_prepare()));
		return mixer;
	}

	assert(false);
	gcc_unreachable();
}

void
FilteredAudioOutput::Configure(const ConfigBlock &block)
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

	{
		char buffer[64];
		snprintf(buffer, sizeof(buffer), "\"%s\" (%s)",
			 name, output->GetPlugin().name);
		log_name = buffer;
	}

	/* set up the filter chain */

	prepared_filter = filter_chain_new();
	assert(prepared_filter != nullptr);

	/* create the normalization filter (if configured) */

	if (config_get_bool(ConfigOption::VOLUME_NORMALIZATION, false)) {
		filter_chain_append(*prepared_filter, "normalize",
				    autoconvert_filter_new(normalize_filter_prepare()));
	}

	try {
		filter_chain_parse(*prepared_filter,
				   block.GetBlockValue(AUDIO_FILTERS, ""));
	} catch (const std::runtime_error &e) {
		/* It's not really fatal - Part of the filter chain
		   has been set up already and even an empty one will
		   work (if only with unexpected behaviour) */
		FormatError(e,
			    "Failed to initialize filter chain for '%s'",
			    name);
	}
}

inline void
FilteredAudioOutput::Setup(EventLoop &event_loop,
			   const ReplayGainConfig &replay_gain_config,
			   MixerListener &mixer_listener,
			   const ConfigBlock &block)
{
	if (output->GetNeedFullyDefinedAudioFormat() &&
	    !config_audio_format.IsFullyDefined())
		throw std::runtime_error("Need full audio format specification");

	/* create the replay_gain filter */

	const char *replay_gain_handler =
		block.GetBlockValue("replay_gain_handler", "software");

	if (strcmp(replay_gain_handler, "none") != 0) {
		prepared_replay_gain_filter =
			NewReplayGainFilter(replay_gain_config);
		assert(prepared_replay_gain_filter != nullptr);

		prepared_other_replay_gain_filter =
			NewReplayGainFilter(replay_gain_config);
		assert(prepared_other_replay_gain_filter != nullptr);
	} else {
		prepared_replay_gain_filter = nullptr;
		prepared_other_replay_gain_filter = nullptr;
	}

	/* set up the mixer */

	try {
		mixer = audio_output_load_mixer(event_loop, *this, block,
						output->GetPlugin().mixer_plugin,
						*prepared_filter,
						mixer_listener);
	} catch (const std::runtime_error &e) {
		FormatError(e,
			    "Failed to initialize hardware mixer for '%s'",
			    name);
	}

	/* use the hardware mixer for replay gain? */

	if (strcmp(replay_gain_handler, "mixer") == 0) {
		if (mixer != nullptr)
			replay_gain_filter_set_mixer(*prepared_replay_gain_filter,
						     mixer, 100);
		else
			FormatError(output_domain,
				    "No such mixer for output '%s'", name);
	} else if (strcmp(replay_gain_handler, "software") != 0 &&
		   prepared_replay_gain_filter != nullptr) {
		throw std::runtime_error("Invalid \"replay_gain_handler\" value");
	}

	/* the "convert" filter must be the last one in the chain */

	filter_chain_append(*prepared_filter, "convert",
			    convert_filter.Set(convert_filter_prepare()));
}

FilteredAudioOutput *
audio_output_new(EventLoop &event_loop,
		 const ReplayGainConfig &replay_gain_config,
		 const ConfigBlock &block,
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
			   "No 'AudioOutput' defined in config file");

		plugin = audio_output_detect();

		FormatDefault(output_domain,
			      "Successfully detected a %s audio device",
			      plugin->name);
	}

	auto *ao = ao_plugin_init(event_loop, *plugin, block);
	assert(ao != nullptr);

	FilteredAudioOutput *f;

	try {
		f = new FilteredAudioOutput(*ao, block);
	} catch (...) {
		ao_plugin_finish(ao);
		throw;
	}

	try {
		f->Setup(event_loop, replay_gain_config,
			 mixer_listener, block);
	} catch (...) {
		delete f;
		throw;
	}

	return f;
}
