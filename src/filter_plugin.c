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

#include "config.h"
#include "filter_plugin.h"
#include "filter_internal.h"
#include "filter_registry.h"
#include "conf.h"

#ifndef NDEBUG
#include "audio_format.h"
#endif

#include <assert.h>

struct filter *
filter_new(const struct filter_plugin *plugin,
	   const struct config_param *param, GError **error_r)
{
	assert(plugin != NULL);
	assert(error_r == NULL || *error_r == NULL);

	return plugin->init(param, error_r);
}

struct filter *
filter_configured_new(const struct config_param *param, GError **error_r)
{
	const char *plugin_name;
	const struct filter_plugin *plugin;

	assert(param != NULL);
	assert(error_r == NULL || *error_r == NULL);

	plugin_name = config_get_block_string(param, "plugin", NULL);
	if (plugin_name == NULL)
		g_set_error(error_r, config_quark(), 0,
			    "No filter plugin specified");

	plugin = filter_plugin_by_name(plugin_name);
	if (plugin == NULL)
		g_set_error(error_r, config_quark(), 0,
			    "No such filter plugin: %s", plugin_name);

	return filter_new(plugin, param, error_r);
}

void
filter_free(struct filter *filter)
{
	assert(filter != NULL);

	filter->plugin->finish(filter);
}

const struct audio_format *
filter_open(struct filter *filter, const struct audio_format *audio_format,
	    GError **error_r)
{
	assert(filter != NULL);
	assert(audio_format != NULL);
	assert(audio_format_valid(audio_format));
	assert(error_r == NULL || *error_r == NULL);

	audio_format = filter->plugin->open(filter, audio_format, error_r);
	assert(audio_format == NULL || audio_format_valid(audio_format));

	return audio_format;
}

void
filter_close(struct filter *filter)
{
	assert(filter != NULL);

	filter->plugin->close(filter);
}

const void *
filter_filter(struct filter *filter, const void *src, size_t src_size,
	      size_t *dest_size_r,
	      GError **error_r)
{
	assert(filter != NULL);
	assert(src != NULL);
	assert(src_size > 0);
	assert(dest_size_r != NULL);
	assert(error_r == NULL || *error_r == NULL);

	return filter->plugin->filter(filter, src, src_size, dest_size_r, error_r);
}
