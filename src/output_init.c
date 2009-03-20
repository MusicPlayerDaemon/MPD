/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "output_control.h"
#include "output_api.h"
#include "output_internal.h"
#include "output_list.h"
#include "audio_parser.h"

#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "output"

#define AUDIO_OUTPUT_TYPE	"type"
#define AUDIO_OUTPUT_NAME	"name"
#define AUDIO_OUTPUT_FORMAT	"format"

static const struct audio_output_plugin *
audio_output_detect(GError **error)
{
	const struct audio_output_plugin *plugin;
	unsigned i;

	g_warning("Attempt to detect audio output device");

	audio_output_plugins_for_each(plugin, i) {
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

bool
audio_output_init(struct audio_output *ao, const struct config_param *param,
		  GError **error)
{
	const char *format;
	const struct audio_output_plugin *plugin = NULL;

	if (param) {
		const char *type = NULL;

		type = config_get_block_string(param, AUDIO_OUTPUT_TYPE, NULL);
		if (type == NULL) {
			g_set_error(error, audio_output_quark(), 0,
				    "Missing \"type\" configuration");
			return false;
		}

		plugin = audio_output_plugin_get(type);
		if (plugin == NULL) {
			g_set_error(error, audio_output_quark(), 0,
				    "No such audio output plugin: %s",
				    type);
			return false;
		}

		ao->name = config_get_block_string(param, AUDIO_OUTPUT_NAME,
						   NULL);
		if (ao->name == NULL) {
			g_set_error(error, audio_output_quark(), 0,
				    "Missing \"name\" configuration");
			return false;
		}

		format = config_get_block_string(param, AUDIO_OUTPUT_FORMAT,
						 NULL);
	} else {
		g_warning("No \"%s\" defined in config file\n",
			  CONF_AUDIO_OUTPUT);

		plugin = audio_output_detect(error);
		if (plugin == NULL)
			return false;

		g_message("Successfully detected a %s audio device",
			  plugin->name);

		ao->name = "default detected output";
		format = NULL;
	}

	ao->plugin = plugin;
	ao->enabled = config_get_block_bool(param, "enabled", true);
	ao->open = false;
	ao->fail_timer = NULL;

	pcm_convert_init(&ao->convert_state);

	ao->config_audio_format = format != NULL;
	if (ao->config_audio_format) {
		bool ret;

		ret = audio_format_parse(&ao->out_audio_format, format,
					 error);
		if (!ret)
			return false;
	}

	ao->thread = NULL;
	notify_init(&ao->notify);
	ao->command = AO_COMMAND_NONE;
	ao->mutex = g_mutex_new();

	ao->data = ao_plugin_init(plugin,
				  ao->config_audio_format
				  ? &ao->out_audio_format : NULL,
				  param, error);
	if (ao->data == NULL)
		return false;

	return true;
}
