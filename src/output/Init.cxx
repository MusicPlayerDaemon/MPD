/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Internal.hxx"
#include "Registry.hxx"
#include "Domain.hxx"
#include "OutputAPI.hxx"
#include "filter/FilterConfig.hxx"
#include "AudioParser.hxx"
#include "mixer/MixerList.hxx"
#include "mixer/MixerType.hxx"
#include "mixer/MixerControl.hxx"
#include "mixer/plugins/SoftwareMixerPlugin.hxx"
#include "filter/FilterPlugin.hxx"
#include "filter/FilterRegistry.hxx"
#include "filter/plugins/AutoConvertFilterPlugin.hxx"
#include "filter/plugins/ReplayGainFilterPlugin.hxx"
#include "filter/plugins/ChainFilterPlugin.hxx"
#include "config/ConfigError.hxx"
#include "config/ConfigGlobal.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"
#define AUDIO_FILTERS		"filters"

static const AudioOutputPlugin *
audio_output_detect(Error &error)
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

	error.Set(output_domain, "Unable to detect an audio device");
	return nullptr;
}

/**
 * Determines the mixer type which should be used for the specified
 * configuration block.
 *
 * This handles the deprecated options mixer_type (global) and
 * mixer_enabled, if the mixer_type setting is not configured.
 */
gcc_pure
static enum mixer_type
audio_output_mixer_type(const config_param &param)
{
	/* read the local "mixer_type" setting */
	const char *p = param.GetBlockValue("mixer_type");
	if (p != nullptr)
		return mixer_type_parse(p);

	/* try the local "mixer_enabled" setting next (deprecated) */
	if (!param.GetBlockValue("mixer_enabled", true))
		return MIXER_TYPE_NONE;

	/* fall back to the global "mixer_type" setting (also
	   deprecated) */
	return mixer_type_parse(config_get_string(CONF_MIXER_TYPE,
						  "hardware"));
}

static Mixer *
audio_output_load_mixer(AudioOutput *ao,
			const config_param &param,
			const struct mixer_plugin *plugin,
			Filter &filter_chain,
			Error &error)
{
	Mixer *mixer;

	switch (audio_output_mixer_type(param)) {
	case MIXER_TYPE_NONE:
	case MIXER_TYPE_UNKNOWN:
		return nullptr;

	case MIXER_TYPE_HARDWARE:
		if (plugin == nullptr)
			return nullptr;

		return mixer_new(plugin, ao, param, error);

	case MIXER_TYPE_SOFTWARE:
		mixer = mixer_new(&software_mixer_plugin, nullptr,
				  config_param(),
				  IgnoreError());
		assert(mixer != nullptr);

		filter_chain_append(filter_chain, "software_mixer",
				    software_mixer_get_filter(mixer));
		return mixer;
	}

	assert(false);
	gcc_unreachable();
}

bool
ao_base_init(AudioOutput *ao,
	     const AudioOutputPlugin *plugin,
	     const config_param &param, Error &error)
{
	assert(ao != nullptr);
	assert(plugin != nullptr);
	assert(plugin->finish != nullptr);
	assert(plugin->open != nullptr);
	assert(plugin->close != nullptr);
	assert(plugin->play != nullptr);

	if (!param.IsNull()) {
		ao->name = param.GetBlockValue(AUDIO_OUTPUT_NAME);
		if (ao->name == nullptr) {
			error.Set(config_domain,
				  "Missing \"name\" configuration");
			return false;
		}

		const char *p = param.GetBlockValue(AUDIO_OUTPUT_FORMAT);
		if (p != nullptr) {
			bool success =
				audio_format_parse(ao->config_audio_format,
						   p, true, error);
			if (!success)
				return false;
		} else
			ao->config_audio_format.Clear();
	} else {
		ao->name = "default detected output";

		ao->config_audio_format.Clear();
	}

	ao->plugin = plugin;
	ao->tags = param.GetBlockValue("tags", true);
	ao->always_on = param.GetBlockValue("always_on", false);
	ao->enabled = param.GetBlockValue("enabled", true);
	ao->really_enabled = false;
	ao->open = false;
	ao->pause = false;
	ao->allow_play = true;
	ao->in_playback_loop = false;
	ao->woken_for_play = false;

	/* set up the filter chain */

	ao->filter = filter_chain_new();
	assert(ao->filter != nullptr);

	/* create the normalization filter (if configured) */

	if (config_get_bool(CONF_VOLUME_NORMALIZATION, false)) {
		Filter *normalize_filter =
			filter_new(&normalize_filter_plugin, config_param(),
				   IgnoreError());
		assert(normalize_filter != nullptr);

		filter_chain_append(*ao->filter, "normalize",
				    autoconvert_filter_new(normalize_filter));
	}

	Error filter_error;
	filter_chain_parse(*ao->filter,
			   param.GetBlockValue(AUDIO_FILTERS, ""),
			   filter_error);

	// It's not really fatal - Part of the filter chain has been set up already
	// and even an empty one will work (if only with unexpected behaviour)
	if (filter_error.IsDefined())
		FormatError(filter_error,
			    "Failed to initialize filter chain for '%s'",
			    ao->name);

	ao->command = AO_COMMAND_NONE;

	ao->mixer = nullptr;
	ao->replay_gain_filter = nullptr;
	ao->other_replay_gain_filter = nullptr;

	/* done */

	return true;
}

static bool
audio_output_setup(AudioOutput *ao, const config_param &param,
		   Error &error)
{

	/* create the replay_gain filter */

	const char *replay_gain_handler =
		param.GetBlockValue("replay_gain_handler", "software");

	if (strcmp(replay_gain_handler, "none") != 0) {
		ao->replay_gain_filter = filter_new(&replay_gain_filter_plugin,
						    param, IgnoreError());
		assert(ao->replay_gain_filter != nullptr);

		ao->replay_gain_serial = 0;

		ao->other_replay_gain_filter = filter_new(&replay_gain_filter_plugin,
							  param,
							  IgnoreError());
		assert(ao->other_replay_gain_filter != nullptr);

		ao->other_replay_gain_serial = 0;
	} else {
		ao->replay_gain_filter = nullptr;
		ao->other_replay_gain_filter = nullptr;
	}

	/* set up the mixer */

	Error mixer_error;
	ao->mixer = audio_output_load_mixer(ao, param,
					    ao->plugin->mixer_plugin,
					    *ao->filter, mixer_error);
	if (ao->mixer == nullptr && mixer_error.IsDefined())
		FormatError(mixer_error,
			    "Failed to initialize hardware mixer for '%s'",
			    ao->name);

	/* use the hardware mixer for replay gain? */

	if (strcmp(replay_gain_handler, "mixer") == 0) {
		if (ao->mixer != nullptr)
			replay_gain_filter_set_mixer(ao->replay_gain_filter,
						     ao->mixer, 100);
		else
			FormatError(output_domain,
				    "No such mixer for output '%s'", ao->name);
	} else if (strcmp(replay_gain_handler, "software") != 0 &&
		   ao->replay_gain_filter != nullptr) {
		error.Set(config_domain,
			  "Invalid \"replay_gain_handler\" value");
		return false;
	}

	/* the "convert" filter must be the last one in the chain */

	ao->convert_filter = filter_new(&convert_filter_plugin, config_param(),
					IgnoreError());
	assert(ao->convert_filter != nullptr);

	filter_chain_append(*ao->filter, "convert", ao->convert_filter);

	return true;
}

AudioOutput *
audio_output_new(const config_param &param,
		 PlayerControl &pc,
		 Error &error)
{
	const AudioOutputPlugin *plugin;

	if (!param.IsNull()) {
		const char *p;

		p = param.GetBlockValue(AUDIO_OUTPUT_TYPE);
		if (p == nullptr) {
			error.Set(config_domain,
				  "Missing \"type\" configuration");
			return nullptr;
		}

		plugin = AudioOutputPlugin_get(p);
		if (plugin == nullptr) {
			error.Format(config_domain,
				     "No such audio output plugin: %s", p);
			return nullptr;
		}
	} else {
		LogWarning(output_domain,
			   "No 'AudioOutput' defined in config file");

		plugin = audio_output_detect(error);
		if (plugin == nullptr)
			return nullptr;

		FormatDefault(output_domain,
			      "Successfully detected a %s audio device",
			      plugin->name);
	}

	AudioOutput *ao = ao_plugin_init(plugin, param, error);
	if (ao == nullptr)
		return nullptr;

	if (!audio_output_setup(ao, param, error)) {
		ao_plugin_finish(ao);
		return nullptr;
	}

	ao->player_control = &pc;
	return ao;
}
