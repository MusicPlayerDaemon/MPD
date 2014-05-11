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

#ifndef MPD_PLAYLIST_PLUGIN_HXX
#define MPD_PLAYLIST_PLUGIN_HXX

struct config_param;
class InputStream;
struct Tag;
class Mutex;
class Cond;
class SongEnumerator;

struct playlist_plugin {
	const char *name;

	/**
	 * Initialize the plugin.  Optional method.
	 *
	 * @param param a configuration block for this plugin, or nullptr
	 * if none is configured
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const config_param &param);

	/**
	 * Deinitialize a plugin which was initialized successfully.
	 * Optional method.
	 */
	void (*finish)(void);

	/**
	 * Opens the playlist on the specified URI.  This URI has
	 * either matched one of the schemes or one of the suffixes.
	 */
	SongEnumerator *(*open_uri)(const char *uri,
				    Mutex &mutex, Cond &cond);

	/**
	 * Opens the playlist in the specified input stream.  It has
	 * either matched one of the suffixes or one of the MIME
	 * types.
	 */
	SongEnumerator *(*open_stream)(InputStream &is);

	const char *const*schemes;
	const char *const*suffixes;
	const char *const*mime_types;
};

/**
 * Initialize a plugin.
 *
 * @param param a configuration block for this plugin, or nullptr if none
 * is configured
 * @return true if the plugin was initialized successfully, false if
 * the plugin is not available
 */
static inline bool
playlist_plugin_init(const struct playlist_plugin *plugin,
		     const config_param &param)
{
	return plugin->init != nullptr
		? plugin->init(param)
		: true;
}

/**
 * Deinitialize a plugin which was initialized successfully.
 */
static inline void
playlist_plugin_finish(const struct playlist_plugin *plugin)
{
	if (plugin->finish != nullptr)
		plugin->finish();
}

static inline SongEnumerator *
playlist_plugin_open_uri(const struct playlist_plugin *plugin, const char *uri,
			 Mutex &mutex, Cond &cond)
{
	return plugin->open_uri(uri, mutex, cond);
}

static inline SongEnumerator *
playlist_plugin_open_stream(const struct playlist_plugin *plugin,
			    InputStream &is)
{
	return plugin->open_stream(is);
}

#endif
