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

#ifndef MPD_PLAYLIST_PLUGIN_H
#define MPD_PLAYLIST_PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

struct config_param;
struct input_stream;
struct tag;

/**
 * An object which provides the contents of a playlist.
 */
struct playlist_provider {
	const struct playlist_plugin *plugin;
};

static inline void
playlist_provider_init(struct playlist_provider *playlist,
		       const struct playlist_plugin *plugin)
{
	playlist->plugin = plugin;
}

struct playlist_plugin {
	const char *name;

	/**
	 * Initialize the plugin.  Optional method.
	 *
	 * @param param a configuration block for this plugin, or NULL
	 * if none is configured
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const struct config_param *param);

	/**
	 * Deinitialize a plugin which was initialized successfully.
	 * Optional method.
	 */
	void (*finish)(void);

	/**
	 * Opens the playlist on the specified URI.  This URI has
	 * either matched one of the schemes or one of the suffixes.
	 */
	struct playlist_provider *(*open_uri)(const char *uri);

	/**
	 * Opens the playlist in the specified input stream.  It has
	 * either matched one of the suffixes or one of the MIME
	 * types.
	 */
	struct playlist_provider *(*open_stream)(struct input_stream *is);

	void (*close)(struct playlist_provider *playlist);

	struct song *(*read)(struct playlist_provider *playlist);

	const char *const*schemes;
	const char *const*suffixes;
	const char *const*mime_types;
};

/**
 * Initialize a plugin.
 *
 * @param param a configuration block for this plugin, or NULL if none
 * is configured
 * @return true if the plugin was initialized successfully, false if
 * the plugin is not available
 */
static inline bool
playlist_plugin_init(const struct playlist_plugin *plugin,
		     const struct config_param *param)
{
	return plugin->init != NULL
		? plugin->init(param)
		: true;
}

/**
 * Deinitialize a plugin which was initialized successfully.
 */
static inline void
playlist_plugin_finish(const struct playlist_plugin *plugin)
{
	if (plugin->finish != NULL)
		plugin->finish();
}

static inline struct playlist_provider *
playlist_plugin_open_uri(const struct playlist_plugin *plugin, const char *uri)
{
	return plugin->open_uri(uri);
}

static inline struct playlist_provider *
playlist_plugin_open_stream(const struct playlist_plugin *plugin,
			    struct input_stream *is)
{
	return plugin->open_stream(is);
}

static inline void
playlist_plugin_close(struct playlist_provider *playlist)
{
	playlist->plugin->close(playlist);
}

static inline struct song *
playlist_plugin_read(struct playlist_provider *playlist)
{
	return playlist->plugin->read(playlist);
}

#endif
