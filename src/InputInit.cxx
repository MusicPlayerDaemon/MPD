/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "InputInit.hxx"
#include "InputRegistry.hxx"
#include "InputPlugin.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "conf.h"

#include <assert.h>
#include <string.h>

extern constexpr Domain input_domain("input");

/**
 * Find the "input" configuration block for the specified plugin.
 *
 * @param plugin_name the name of the input plugin
 * @return the configuration block, or NULL if none was configured
 */
static const struct config_param *
input_plugin_config(const char *plugin_name, Error &error)
{
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_INPUT, param)) != NULL) {
		const char *name = param->GetBlockValue("plugin");
		if (name == NULL) {
			error.Format(input_domain,
				     "input configuration without 'plugin' name in line %d",
				     param->line);
			return NULL;
		}

		if (strcmp(name, plugin_name) == 0)
			return param;
	}

	return NULL;
}

bool
input_stream_global_init(Error &error)
{
	const config_param empty;

	for (unsigned i = 0; input_plugins[i] != NULL; ++i) {
		const struct input_plugin *plugin = input_plugins[i];

		assert(plugin->name != NULL);
		assert(*plugin->name != 0);
		assert(plugin->open != NULL);

		const struct config_param *param =
			input_plugin_config(plugin->name, error);
		if (param == nullptr) {
			if (error.IsDefined())
				return false;

			param = &empty;
		} else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (plugin->init == NULL || plugin->init(*param, error))
			input_plugins_enabled[i] = true;
		else {
			error.FormatPrefix("Failed to initialize input plugin '%s': ",
					   plugin->name);
			return false;
		}
	}

	return true;
}

void input_stream_global_finish(void)
{
	input_plugins_for_each_enabled(plugin)
		if (plugin->finish != NULL)
			plugin->finish();
}
