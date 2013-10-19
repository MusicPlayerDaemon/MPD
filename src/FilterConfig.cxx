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
#include "FilterConfig.hxx"
#include "filter/ChainFilterPlugin.hxx"
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "ConfigData.hxx"
#include "ConfigOption.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigError.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <string.h>

/**
 * Find the "filter" configuration block for the specified name.
 *
 * @param filter_template_name the name of the filter template
 * @param error space to return an error description
 * @return the configuration block, or NULL if none was configured
 */
static const struct config_param *
filter_plugin_config(const char *filter_template_name, Error &error)
{
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_AUDIO_FILTER, param)) != NULL) {
		const char *name = param->GetBlockValue("name");
		if (name == NULL) {
			error.Format(config_domain,
				     "filter configuration without 'name' name in line %d",
				     param->line);
			return NULL;
		}

		if (strcmp(name, filter_template_name) == 0)
			return param;
	}

	error.Format(config_domain,
		     "filter template not found: %s",
		     filter_template_name);
	return NULL;
}

static bool
filter_chain_append_new(Filter &chain, const char *template_name, Error &error)
{
	const struct config_param *cfg =
		filter_plugin_config(template_name, error);
	if (cfg == nullptr)
		// The error has already been set, just stop.
		return false;

	// Instantiate one of those filter plugins with the template name as a hint
	Filter *f = filter_configured_new(*cfg, error);
	if (f == nullptr)
		// The error has already been set, just stop.
		return false;

	const char *plugin_name = cfg->GetBlockValue("plugin",
						     "unknown");
	filter_chain_append(chain, plugin_name, f);

	return true;
}

bool
filter_chain_parse(Filter &chain, const char *spec, Error &error)
{
	// Split on comma
	gchar** tokens = g_strsplit_set(spec, ",", 255);

	// Add each name to the filter chain by instantiating an actual filter
	char **template_names = tokens;
	while (*template_names != NULL) {
		// Squeeze whitespace
		g_strstrip(*template_names);

		if (!filter_chain_append_new(chain, *template_names, error))
			return false;

		++template_names;
	}

	g_strfreev(tokens);

	return true;
}
