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
#include "FilterPlugin.hxx"
#include "FilterInternal.hxx"
#include "FilterRegistry.hxx"
#include "ConfigData.hxx"
#include "ConfigError.hxx"
#include "util/Error.hxx"

#include <assert.h>

Filter *
filter_new(const struct filter_plugin *plugin,
	   const config_param &param, Error &error)
{
	assert(plugin != NULL);
	assert(!error.IsDefined());

	return plugin->init(param, error);
}

Filter *
filter_configured_new(const config_param &param, Error &error)
{
	assert(!error.IsDefined());

	const char *plugin_name = param.GetBlockValue("plugin");
	if (plugin_name == NULL) {
		error.Set(config_domain, "No filter plugin specified");
		return NULL;
	}

	const filter_plugin *plugin = filter_plugin_by_name(plugin_name);
	if (plugin == NULL) {
		error.Format(config_domain,
			     "No such filter plugin: %s", plugin_name);
		return NULL;
	}

	return filter_new(plugin, param, error);
}
