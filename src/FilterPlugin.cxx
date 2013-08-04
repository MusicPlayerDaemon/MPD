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
#include "conf.h"
#include "ConfigQuark.hxx"

#include <assert.h>

Filter *
filter_new(const struct filter_plugin *plugin,
	   const config_param &param, GError **error_r)
{
	assert(plugin != NULL);
	assert(error_r == NULL || *error_r == NULL);

	return plugin->init(param, error_r);
}

Filter *
filter_configured_new(const config_param &param, GError **error_r)
{
	const struct filter_plugin *plugin;

	assert(error_r == NULL || *error_r == NULL);

	const char *plugin_name = param.GetBlockValue("plugin");
	if (plugin_name == NULL) {
		g_set_error(error_r, config_quark(), 0,
			    "No filter plugin specified");
		return NULL;
	}

	plugin = filter_plugin_by_name(plugin_name);
	if (plugin == NULL) {
		g_set_error(error_r, config_quark(), 0,
			    "No such filter plugin: %s", plugin_name);
		return NULL;
	}

	return filter_new(plugin, param, error_r);
}
