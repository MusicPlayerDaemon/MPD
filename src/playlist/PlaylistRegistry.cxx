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
#include "PlaylistRegistry.hxx"
#include "PlaylistPlugin.hxx"
#include "plugins/ExtM3uPlaylistPlugin.hxx"
#include "plugins/M3uPlaylistPlugin.hxx"
#include "plugins/XspfPlaylistPlugin.hxx"
#include "plugins/SoundCloudPlaylistPlugin.hxx"
#include "plugins/PlsPlaylistPlugin.hxx"
#include "plugins/AsxPlaylistPlugin.hxx"
#include "plugins/RssPlaylistPlugin.hxx"
#include "plugins/CuePlaylistPlugin.hxx"
#include "plugins/EmbeddedCuePlaylistPlugin.hxx"
#include "input/InputStream.hxx"
#include "util/UriUtil.hxx"
#include "util/StringUtil.hxx"
#include "util/Error.hxx"
#include "util/Macros.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "Log.hxx"

#include <assert.h>
#include <string.h>

const struct playlist_plugin *const playlist_plugins[] = {
	&extm3u_playlist_plugin,
	&m3u_playlist_plugin,
#ifdef HAVE_GLIB
	// TODO: enable without GLib
	&pls_playlist_plugin,
#endif
#ifdef HAVE_EXPAT
	&xspf_playlist_plugin,
	&asx_playlist_plugin,
	&rss_playlist_plugin,
#endif
#ifdef ENABLE_SOUNDCLOUD
	&soundcloud_playlist_plugin,
#endif
	&cue_playlist_plugin,
	&embcue_playlist_plugin,
	nullptr
};

static constexpr unsigned n_playlist_plugins =
	ARRAY_SIZE(playlist_plugins) - 1;

/** which plugins have been initialized successfully? */
static bool playlist_plugins_enabled[n_playlist_plugins];

#define playlist_plugins_for_each_enabled(plugin) \
	playlist_plugins_for_each(plugin) \
		if (playlist_plugins_enabled[playlist_plugin_iterator - playlist_plugins])

void
playlist_list_global_init(void)
{
	const config_param empty;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];
		const struct config_param *param =
			config_find_block(CONF_PLAYLIST_PLUGIN, "name",
					  plugin->name);
		if (param == nullptr)
			param = &empty;
		else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		playlist_plugins_enabled[i] =
			playlist_plugin_init(playlist_plugins[i], *param);
	}
}

void
playlist_list_global_finish(void)
{
	playlist_plugins_for_each_enabled(plugin)
		playlist_plugin_finish(plugin);
}

static SongEnumerator *
playlist_list_open_uri_scheme(const char *uri, Mutex &mutex, Cond &cond,
			      bool *tried)
{
	SongEnumerator *playlist = nullptr;

	assert(uri != nullptr);

	const auto scheme = uri_get_scheme(uri);
	if (scheme.empty())
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		assert(!tried[i]);

		if (playlist_plugins_enabled[i] && plugin->open_uri != nullptr &&
		    plugin->schemes != nullptr &&
		    string_array_contains(plugin->schemes, scheme.c_str())) {
			playlist = playlist_plugin_open_uri(plugin, uri,
							    mutex, cond);
			if (playlist != nullptr)
				break;

			tried[i] = true;
		}
	}

	return playlist;
}

static SongEnumerator *
playlist_list_open_uri_suffix(const char *uri, Mutex &mutex, Cond &cond,
			      const bool *tried)
{
	SongEnumerator *playlist = nullptr;

	assert(uri != nullptr);

	UriSuffixBuffer suffix_buffer;
	const char *const suffix = uri_get_suffix(uri, suffix_buffer);
	if (suffix == nullptr)
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const struct playlist_plugin *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && !tried[i] &&
		    plugin->open_uri != nullptr && plugin->suffixes != nullptr &&
		    string_array_contains(plugin->suffixes, suffix)) {
			playlist = playlist_plugin_open_uri(plugin, uri,
							    mutex, cond);
			if (playlist != nullptr)
				break;
		}
	}

	return playlist;
}

SongEnumerator *
playlist_list_open_uri(const char *uri, Mutex &mutex, Cond &cond)
{
	/** this array tracks which plugins have already been tried by
	    playlist_list_open_uri_scheme() */
	bool tried[n_playlist_plugins];

	assert(uri != nullptr);

	memset(tried, false, sizeof(tried));

	auto playlist = playlist_list_open_uri_scheme(uri, mutex, cond, tried);
	if (playlist == nullptr)
		playlist = playlist_list_open_uri_suffix(uri, mutex, cond,
							 tried);

	return playlist;
}

static SongEnumerator *
playlist_list_open_stream_mime2(InputStream &is, const char *mime)
{
	assert(mime != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->mime_types != nullptr &&
		    string_array_contains(plugin->mime_types, mime)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			is.Rewind(IgnoreError());

			auto playlist = playlist_plugin_open_stream(plugin,
								    is);
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

static SongEnumerator *
playlist_list_open_stream_mime(InputStream &is, const char *full_mime)
{
	assert(full_mime != nullptr);

	const char *semicolon = strchr(full_mime, ';');
	if (semicolon == nullptr)
		return playlist_list_open_stream_mime2(is, full_mime);

	if (semicolon == full_mime)
		return nullptr;

	/* probe only the portion before the semicolon*/
	const std::string mime(full_mime, semicolon);
	return playlist_list_open_stream_mime2(is, mime.c_str());
}

SongEnumerator *
playlist_list_open_stream_suffix(InputStream &is, const char *suffix)
{
	assert(suffix != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->suffixes != nullptr &&
		    string_array_contains(plugin->suffixes, suffix)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			is.Rewind(IgnoreError());

			auto playlist = playlist_plugin_open_stream(plugin, is);
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

SongEnumerator *
playlist_list_open_stream(InputStream &is, const char *uri)
{
	assert(is.IsReady());

	const char *const mime = is.GetMimeType();
	if (mime != nullptr) {
		auto playlist = playlist_list_open_stream_mime(is, mime);
		if (playlist != nullptr)
			return playlist;
	}

	UriSuffixBuffer suffix_buffer;
	const char *suffix = uri != nullptr
		? uri_get_suffix(uri, suffix_buffer)
		: nullptr;
	if (suffix != nullptr) {
		auto playlist = playlist_list_open_stream_suffix(is, suffix);
		if (playlist != nullptr)
			return playlist;
	}

	return nullptr;
}

bool
playlist_suffix_supported(const char *suffix)
{
	assert(suffix != nullptr);

	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->suffixes != nullptr &&
		    string_array_contains(plugin->suffixes, suffix))
			return true;
	}

	return false;
}
