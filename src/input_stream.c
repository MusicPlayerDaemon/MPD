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
#include "input_plugin.h"
#include "conf.h"

#include "input/file_input_plugin.h"

#ifdef ENABLE_ARCHIVE
#include "input/archive_input_plugin.h"
#endif

#ifdef ENABLE_CURL
#include "input/curl_input_plugin.h"
#endif

#ifdef ENABLE_MMS
#include "input/mms_input_plugin.h"
#endif

#include <glib.h>
#include <assert.h>
#include <string.h>

static const struct input_plugin *const input_plugins[] = {
	&input_plugin_file,
#ifdef ENABLE_ARCHIVE
	&input_plugin_archive,
#endif
#ifdef ENABLE_CURL
	&input_plugin_curl,
#endif
#ifdef ENABLE_MMS
	&input_plugin_mms,
#endif
};

static bool input_plugins_enabled[G_N_ELEMENTS(input_plugins)];

static const unsigned num_input_plugins =
	sizeof(input_plugins) / sizeof(input_plugins[0]);

/**
 * Find the "input" configuration block for the specified plugin.
 *
 * @param plugin_name the name of the input plugin
 * @return the configuration block, or NULL if none was configured
 */
static const struct config_param *
input_plugin_config(const char *plugin_name)
{
	const struct config_param *param = NULL;

	while ((param = config_get_next_param(CONF_INPUT, param)) != NULL) {
		const char *name =
			config_get_block_string(param, "plugin", NULL);
		if (name == NULL)
			g_error("input configuration without 'plugin' name in line %d",
				param->line);

		if (strcmp(name, plugin_name) == 0)
			return param;
	}

	return NULL;
}

void input_stream_global_init(void)
{
	for (unsigned i = 0; i < num_input_plugins; ++i) {
		const struct input_plugin *plugin = input_plugins[i];
		const struct config_param *param =
			input_plugin_config(plugin->name);

		if (!config_get_block_bool(param, "enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (plugin->init == NULL || plugin->init(param))
			input_plugins_enabled[i] = true;
	}
}

void input_stream_global_finish(void)
{
	for (unsigned i = 0; i < num_input_plugins; ++i)
		if (input_plugins_enabled[i] &&
		    input_plugins[i]->finish != NULL)
			input_plugins[i]->finish();
}

bool
input_stream_open(struct input_stream *is, const char *url)
{
	is->seekable = false;
	is->ready = false;
	is->offset = 0;
	is->size = -1;
	is->error = 0;
	is->mime = NULL;

	for (unsigned i = 0; i < num_input_plugins; ++i) {
		const struct input_plugin *plugin = input_plugins[i];

		if (input_plugins_enabled[i] && plugin->open(is, url)) {
			assert(is->plugin != NULL);
			assert(is->plugin->close != NULL);
			assert(is->plugin->read != NULL);
			assert(is->plugin->eof != NULL);
			assert(!is->seekable || is->plugin->seek != NULL);

			return true;
		}
	}

	return false;
}

bool
input_stream_seek(struct input_stream *is, goffset offset, int whence)
{
	if (is->plugin->seek == NULL)
		return false;

	return is->plugin->seek(is, offset, whence);
}

struct tag *
input_stream_tag(struct input_stream *is)
{
	assert(is != NULL);

	return is->plugin->tag != NULL
		? is->plugin->tag(is)
		: NULL;
}

size_t
input_stream_read(struct input_stream *is, void *ptr, size_t size)
{
	assert(ptr != NULL);
	assert(size > 0);

	return is->plugin->read(is, ptr, size);
}

void input_stream_close(struct input_stream *is)
{
	is->plugin->close(is);

	g_free(is->mime);
}

bool input_stream_eof(struct input_stream *is)
{
	return is->plugin->eof(is);
}

int input_stream_buffer(struct input_stream *is)
{
	if (is->plugin->buffer == NULL)
		return 0;

	return is->plugin->buffer(is);
}
