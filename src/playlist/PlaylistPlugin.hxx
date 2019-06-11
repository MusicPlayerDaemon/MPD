/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"

struct ConfigBlock;
struct Tag;
class SongEnumerator;

struct playlist_plugin {
	const char *name;

	/**
	 * Initialize the plugin.  Optional method.
	 *
	 * @param block a configuration block for this plugin (may be
	 * empty if none is configured)
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const ConfigBlock &block);

	/**
	 * Deinitialize a plugin which was initialized successfully.
	 * Optional method.
	 */
	void (*finish)();

	/**
	 * Opens the playlist on the specified URI.  This URI has
	 * either matched one of the schemes or one of the suffixes.
	 */
	std::unique_ptr<SongEnumerator> (*open_uri)(const char *uri,
						    Mutex &mutex);

	/**
	 * Opens the playlist in the specified input stream.  It has
	 * either matched one of the suffixes or one of the MIME
	 * types.
	 *
	 * @parm is the input stream; the pointer will not be
	 * invalidated when the function returns nullptr
	 */
	std::unique_ptr<SongEnumerator> (*open_stream)(InputStreamPtr &&is);

	const char *const*schemes;
	const char *const*suffixes;
	const char *const*mime_types;
};

/**
 * Initialize a plugin.
 *
 * @param block a configuration block for this plugin, or nullptr if none
 * is configured
 * @return true if the plugin was initialized successfully, false if
 * the plugin is not available
 */
static inline bool
playlist_plugin_init(const struct playlist_plugin *plugin,
		     const ConfigBlock &block)
{
	return plugin->init != nullptr
		? plugin->init(block)
		: true;
}

/**
 * Deinitialize a plugin which was initialized successfully.
 */
static inline void
playlist_plugin_finish(const struct playlist_plugin *plugin) noexcept
{
	if (plugin->finish != nullptr)
		plugin->finish();
}

#endif
