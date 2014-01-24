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
#include "FilterConfig.hxx"
#include "plugins/ChainFilterPlugin.hxx"
#include "FilterPlugin.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigOption.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"

#include <algorithm>

#include <string.h>

static bool
filter_chain_append_new(Filter &chain, const char *template_name, Error &error)
{
	const struct config_param *cfg =
		config_find_block(CONF_AUDIO_FILTER, "name", template_name);
	if (cfg == nullptr) {
		error.Format(config_domain,
			     "filter template not found: %s",
			     template_name);
		return false;
	}

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
	const char *const end = spec + strlen(spec);

	while (true) {
		const char *comma = std::find(spec, end, ',');
		if (comma > spec) {
			const std::string name(spec, comma);
			if (!filter_chain_append_new(chain, name.c_str(),
						     error))
				return false;
		}

		if (comma == end)
			break;

		spec = comma + 1;
	}

	return true;
}
