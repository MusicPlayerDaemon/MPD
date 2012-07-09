/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_list.h"
#include "audio_parser.h"
#include "mixer_control.h"
#include "mixer_type.h"
#include "mixer_list.h"
#include "mixer/software_mixer_plugin.h"
#include "filter_plugin.h"
#include "filter_registry.h"
#include "filter_config.h"
#include "filter/chain_filter_plugin.h"
#include "filter/autoconvert_filter_plugin.h"
#include "filter/replay_gain_filter_plugin.h"

#include <glib.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "output"

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"
#define AUDIO_FILTERS		"filters"

static const struct audio_output_plugin *
audio_output_detect(GError **error)
{
	g_warning("Attempt to detect audio output device");

	audio_output_plugins_for_each(plugin) {
		if (plugin->test_default_device == NULL)
			continue;

		g_warning("Attempting to detect a %s audio device",
			  plugin->name);
		if (ao_plugin_test_default_device(plugin))
			return plugin;
	}

	g_set_error(error, audio_output_quark(), 0,
		    "Unable to detect an audio device");
	return NULL;
}

/**
 * Determines the mixer type which should be used for the specified
 * configuration block.
 *
 * This handles the deprecated options mixer_type (global) and
 * mixer_enabled, if the mixer_type setting is not configured.
 */
static enum mixer_type
audio_output_mixer_type(const struct config_param *param)
{
	/* read the local "mixer_type" setting */
	const char *p = config_get_block_string(param, "mixer_type", NULL);
	if (p != NULL)
		return mixer_type_parse(p);

	/* try the local "mixer_enabled" setting next (deprecated) */
	if (!config_get_block_bool(param, "mixer_enabled", true))
		return MIXER_TYPE_NONE;

	/* fall back to the global "mixer_type" setting (also
	   deprecated) */
	return mixer_type_parse(config_get_string("mixer_type", "hardware"));
}

static struct mixer *
audio_output_load_mixer(struct audio_output *ao,
			const struct config_param *param,
			const struct mixer_plugin *plugin,
			struct filter *filter_chain,
			GError **error_r)
{
	struct mixer *mixer;

	switch (audio_output_mixer_type(param)) {
	case MIXER_TYPE_NONE:
	case MIXER_TYPE_UNKNOWN:
		return NULL;

	case MIXER_TYPE_HARDWARE:
		if (plugin == NULL)
			return NULL;

		return mixer_new(plugin, ao, param, error_r);

	case MIXER_TYPE_SOFTWARE:
		mixer = mixer_new(&software_mixer_plugin, NULL, NULL, NULL);
		assert(mixer != NULL);

		filter_chain_append(filter_chain,
				    software_mixer_get_filter(mixer));
		return mixer;
	}

	assert(false);
	return NULL;
}

bool
ao_base_init(struct audio_output *ao,
	     const struct audio_output_plugin *plugin,
	     const struct config_param *param, GError **error_r)
{
	assert(ao != NULL);
	assert(plugin != NULL);
	assert(plugin->finish != NULL);
	assert(plugin->open != NULL);
	assert(plugin->close != NULL);
	assert(plugin->play != NULL);

	GError *error = NULL;

	if (param) {
		const char *p;

		ao->name = config_get_block_string(param, AUDIO_OUTPUT_NAME,
						   NULL);
		if (ao->name == NULL) {
			g_set_error(error_r, audio_output_quark(), 0,
				    "Missing \"name\" configuration");
			return false;
		}

		p = config_get_block_string(param, AUDIO_OUTPUT_FORMAT,
						 NULL);
		if (p != NULL) {
			bool success =
				audio_format_parse(&ao->config_audio_format,
						   p, true, error_r);
			if (!success)
				return false;
		} else
			audio_format_clear(&ao->config_audio_format);
	} else {
		ao->name = "default detected output";

		audio_format_clear(&ao->config_audio_format);
	}

	ao->plugin = plugin;
	ao->always_on = config_get_block_bool(param, "always_on", false);
	ao->enabled = config_get_block_bool(param, "enabled", true);
	ao->really_enabled = false;
	ao->open = false;
	ao->pause = false;
	ao->allow_play = true;
	ao->fail_timer = NULL;

	pcm_buffer_init(&ao->cross_fade_buffer);

	/* set up the filter chain */

	ao->filter = filter_chain_new();
	assert(ao->filter != NULL);

	/* create the normalization filter (if configured) */

	if (config_get_bool(CONF_VOLUME_NORMALIZATION, false)) {
		struct filter *normalize_filter =
			filter_new(&normalize_filter_plugin, NULL, NULL);
		assert(normalize_filter != NULL);

		filter_chain_append(ao->filter,
				    autoconvert_filter_new(normalize_filter));
	}

	filter_chain_parse(ao->filter,
	                   config_get_block_string(param, AUDIO_FILTERS, ""),
	                   &error
	);

	// It's not really fatal - Part of the filter chain has been set up already
	// and even an empty one will work (if only with unexpected behaviour)
	if (error != NULL) {
		g_warning("Failed to initialize filter chain for '%s': %s",
			  ao->name, error->message);
		g_error_free(error);
	}

	ao->thread = NULL;
	ao->command = AO_COMMAND_NONE;
	ao->mutex = g_mutex_new();
	ao->cond = g_cond_new();

	ao->mixer = NULL;
	ao->replay_gain_filter = NULL;
	ao->other_replay_gain_filter = NULL;

	/* done */

	return true;
}

static bool
audio_output_setup(struct audio_output *ao, const struct config_param *param,
		   GError **error_r)
{

	/* create the replay_gain filter */

	const char *replay_gain_handler =
		config_get_block_string(param, "replay_gain_handler",
					"software");

	if (strcmp(replay_gain_handler, "none") != 0) {
		ao->replay_gain_filter = filter_new(&replay_gain_filter_plugin,
						    param, NULL);
		assert(ao->replay_gain_filter != NULL);

		ao->replay_gain_serial = 0;

		ao->other_replay_gain_filter = filter_new(&replay_gain_filter_plugin,
							  param, NULL);
		assert(ao->other_replay_gain_filter != NULL);

		ao->other_replay_gain_serial = 0;
	} else {
		ao->replay_gain_filter = NULL;
		ao->other_replay_gain_filter = NULL;
	}

	/* set up the mixer */

	GError *error = NULL;
	ao->mixer = audio_output_load_mixer(ao, param,
					    ao->plugin->mixer_plugin,
					    ao->filter, &error);
	if (ao->mixer == NULL && error != NULL) {
		g_warning("Failed to initialize hardware mixer for '%s': %s",
			  ao->name, error->message);
		g_error_free(error);
	}

	/* use the hardware mixer for replay gain? */

	if (strcmp(replay_gain_handler, "mixer") == 0) {
		if (ao->mixer != NULL)
			replay_gain_filter_set_mixer(ao->replay_gain_filter,
						     ao->mixer, 100);
		else
			g_warning("No such mixer for output '%s'", ao->name);
	} else if (strcmp(replay_gain_handler, "software") != 0 &&
		   ao->replay_gain_filter != NULL) {
		g_set_error(error_r, audio_output_quark(), 0,
			    "Invalid \"replay_gain_handler\" value");
		return false;
	}

	/* the "convert" filter must be the last one in the chain */

	ao->convert_filter = filter_new(&convert_filter_plugin, NULL, NULL);
	assert(ao->convert_filter != NULL);

	filter_chain_append(ao->filter, ao->convert_filter);

	return true;
}

struct audio_output *
audio_output_new(const struct config_param *param,
		 struct player_control *pc,
		 GError **error_r)
{
	const struct audio_output_plugin *plugin;

	if (param) {
		const char *p;

		p = config_get_block_string(param, AUDIO_OUTPUT_TYPE, NULL);
		if (p == NULL) {
			g_set_error(error_r, audio_output_quark(), 0,
				    "Missing \"type\" configuration");
			return false;
		}

		plugin = audio_output_plugin_get(p);
		if (plugin == NULL) {
			g_set_error(error_r, audio_output_quark(), 0,
				    "No such audio output plugin: %s", p);
			return false;
		}
	} else {
		g_warning("No \"%s\" defined in config file\n",
			  CONF_AUDIO_OUTPUT);

		plugin = audio_output_detect(error_r);
		if (plugin == NULL)
			return false;

		g_message("Successfully detected a %s audio device",
			  plugin->name);
	}

	struct audio_output *ao = ao_plugin_init(plugin, param, error_r);
	if (ao == NULL)
		return NULL;

	if (!audio_output_setup(ao, param, error_r)) {
		ao_plugin_finish(ao);
		return NULL;
	}

	ao->player_control = pc;
	return ao;
}
