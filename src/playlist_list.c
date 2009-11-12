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
#include "playlist_list.h"
#include "playlist_plugin.h"
#include "playlist/extm3u_playlist_plugin.h"
#include "playlist/m3u_playlist_plugin.h"
#include "playlist/xspf_playlist_plugin.h"
#include "playlist/lastfm_playlist_plugin.h"
#include "playlist/pls_playlist_plugin.h"
#include "playlist/asx_playlist_plugin.h"
#include "input_stream.h"
#include "uri.h"
#include "utils.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

static const struct playlist_plugin *const playlist_plugins[] = {
	&extm3u_playlist_plugin,
	&m3u_playlist_plugin,
	&xspf_playlist_plugin,
	&pls_playlist_plugin,
	&asx_playlist_plugin,
#ifdef ENABLE_LASTFM
	&lastfm_playlist_plugin,
#endif
	NULL
};

/** which plugins have been initialized successfully? */
static bool playlist_plugins_enabled[G_N_ELEMENTS(playlist_plugins)];

/**
 * Find the "playlist" configuration block for the specified plugin.
 *
 * @param plugin_name the name of the playlist plugin
 * @return the configuration block, or NULL if none was configured
 */
static const struct config_param *
playlist_plugin_config(const char *plugin_name)
{
	const struct config_param *param = NULL;

	assert(plugin_name != NULL);

	while ((param = config_get_next_param(CONF_PLAYLIST_PLUGIN, param)) != NULL) {
		const char *name =
			config_get_block_string(param, "name", NULL);
		if (name == NULL)
			g_error("playlist configuration without 'plugin' name in line %d",
				param->line);

		if (strcmp(name, plugin_name) == 0)
			return param;
	}

	return NULL;
}

void
playlist_list_global_init(void)
{
	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];
		const struct config_param *param =
			playlist_plugin_config(plugin->name);

		if (!config_get_block_bool(param, "enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		playlist_plugins_enabled[i] =
			playlist_plugin_init(playlist_plugins[i], param);
	}
}

void
playlist_list_global_finish(void)
{
	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i)
		if (playlist_plugins_enabled[i])
			playlist_plugin_finish(playlist_plugins[i]);
}

/* g_uri_parse_scheme() was introduced in GLib 2.16 */
#if !GLIB_CHECK_VERSION(2,16,0)
static char *
g_uri_parse_scheme(const char *uri)
{
	const char *end = strstr(uri, "://");
	if (end == NULL)
		return NULL;
	return g_strndup(uri, end - uri);
}
#endif

struct playlist_provider *
playlist_list_open_uri(const char *uri)
{
	char *scheme;
	struct playlist_provider *playlist = NULL;

	assert(uri != NULL);

	scheme = g_uri_parse_scheme(uri);
	if (scheme == NULL)
		return NULL;

	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && plugin->schemes != NULL &&
		    string_array_contains(plugin->schemes, scheme)) {
			playlist = playlist_plugin_open_uri(plugin, uri);
			if (playlist != NULL)
				break;
		}
	}

	g_free(scheme);
	return playlist;
}

static struct playlist_provider *
playlist_list_open_stream_mime(struct input_stream *is)
{
	struct playlist_provider *playlist;

	assert(is != NULL);
	assert(is->mime != NULL);

	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] &&
		    plugin->mime_types != NULL &&
		    string_array_contains(plugin->mime_types, is->mime)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			input_stream_seek(is, 0, SEEK_SET);

			playlist = playlist_plugin_open_stream(plugin, is);
			if (playlist != NULL)
				return playlist;
		}
	}

	return NULL;
}

static struct playlist_provider *
playlist_list_open_stream_suffix(struct input_stream *is, const char *suffix)
{
	struct playlist_provider *playlist;

	assert(is != NULL);
	assert(suffix != NULL);

	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && plugin->suffixes != NULL &&
		    string_array_contains(plugin->suffixes, suffix)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			input_stream_seek(is, 0, SEEK_SET);

			playlist = playlist_plugin_open_stream(plugin, is);
			if (playlist != NULL)
				return playlist;
		}
	}

	return NULL;
}

struct playlist_provider *
playlist_list_open_stream(struct input_stream *is, const char *uri)
{
	const char *suffix;
	struct playlist_provider *playlist;

	if (is->mime != NULL) {
		playlist = playlist_list_open_stream_mime(is);
		if (playlist != NULL)
			return playlist;
	}

	suffix = uri != NULL ? uri_get_suffix(uri) : NULL;
	if (suffix != NULL) {
		playlist = playlist_list_open_stream_suffix(is, suffix);
		if (playlist != NULL)
			return playlist;
	}

	return NULL;
}

static bool
playlist_suffix_supported(const char *suffix)
{
	assert(suffix != NULL);

	for (unsigned i = 0; playlist_plugins[i] != NULL; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && plugin->suffixes != NULL &&
		    string_array_contains(plugin->suffixes, suffix))
			return true;
	}

	return false;
}

struct playlist_provider *
playlist_list_open_path(struct input_stream *is, const char *path_fs)
{
	const char *suffix;
	struct playlist_provider *playlist;

	assert(path_fs != NULL);

	suffix = uri_get_suffix(path_fs);
	if (suffix == NULL || !playlist_suffix_supported(suffix) ||
	    !input_stream_open(is, path_fs))
		return NULL;

	playlist = playlist_list_open_stream_suffix(is, suffix);
	if (playlist == NULL)
		input_stream_close(is);

	return playlist;
}
