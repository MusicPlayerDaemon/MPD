/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "SongEnumerator.hxx"
#include "playlist/Features.h"
#include "plugins/ExtM3uPlaylistPlugin.hxx"
#include "plugins/M3uPlaylistPlugin.hxx"
#include "plugins/XspfPlaylistPlugin.hxx"
#include "plugins/SoundCloudPlaylistPlugin.hxx"
#include "plugins/PlsPlaylistPlugin.hxx"
#include "plugins/AsxPlaylistPlugin.hxx"
#include "plugins/RssPlaylistPlugin.hxx"
#include "plugins/FlacPlaylistPlugin.hxx"
#include "plugins/CuePlaylistPlugin.hxx"
#include "plugins/EmbeddedCuePlaylistPlugin.hxx"
#include "decoder/Features.h"
#include "input/InputStream.hxx"
#include "util/MimeType.hxx"
#include "util/StringView.hxx"
#include "util/UriExtract.hxx"
#include "config/Data.hxx"
#include "config/Block.hxx"

#include <cassert>
#include <iterator>

constexpr const PlaylistPlugin *playlist_plugins[] = {
	&extm3u_playlist_plugin,
	&m3u_playlist_plugin,
	&pls_playlist_plugin,
#ifdef ENABLE_EXPAT
	&xspf_playlist_plugin,
	&asx_playlist_plugin,
	&rss_playlist_plugin,
#endif
#ifdef ENABLE_SOUNDCLOUD
	&soundcloud_playlist_plugin,
#endif
#ifdef ENABLE_FLAC
	&flac_playlist_plugin,
#endif
#ifdef ENABLE_CUE
	&cue_playlist_plugin,
	&embcue_playlist_plugin,
#endif
	nullptr
};

static constexpr unsigned n_playlist_plugins =
	std::size(playlist_plugins) - 1;

/** which plugins have been initialized successfully? */
static bool playlist_plugins_enabled[n_playlist_plugins];

/** which plugins have the "as_folder" option enabled? */
static bool playlist_plugins_as_folder[n_playlist_plugins];

#define playlist_plugins_for_each_enabled(plugin) \
	playlist_plugins_for_each(plugin) \
		if (playlist_plugins_enabled[playlist_plugin_iterator - playlist_plugins])

void
playlist_list_global_init(const ConfigData &config)
{
	const ConfigBlock empty;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const auto *plugin = playlist_plugins[i];
		const auto *param =
			config.FindBlock(ConfigBlockOption::PLAYLIST_PLUGIN,
					 "name", plugin->name);
		if (param == nullptr)
			param = &empty;
		else if (!param->GetBlockValue("enabled", true))
			/* the plugin is disabled in mpd.conf */
			continue;

		if (param != nullptr)
			param->SetUsed();

		playlist_plugins_enabled[i] =
			playlist_plugin_init(playlist_plugins[i], *param);

		playlist_plugins_as_folder[i] =
			param->GetBlockValue("as_directory",
					     playlist_plugins[i]->as_folder);
	}
}

void
playlist_list_global_finish() noexcept
{
	playlist_plugins_for_each_enabled(plugin)
		playlist_plugin_finish(plugin);
}

bool
GetPlaylistPluginAsFolder(const PlaylistPlugin &plugin) noexcept
{
	/* this loop has no end condition because it must finish when
	   the plugin was found */
	for (std::size_t i = 0;; ++i)
		if (playlist_plugins[i] == &plugin)
			return playlist_plugins_as_folder[i];
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_uri_scheme(const char *uri, Mutex &mutex,
			      bool *tried)
{
	assert(uri != nullptr);

	const auto scheme = uri_get_scheme(uri);
	if (scheme.empty())
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const auto *plugin = playlist_plugins[i];

		assert(!tried[i]);

		if (playlist_plugins_enabled[i] && plugin->open_uri != nullptr &&
		    plugin->SupportsScheme(scheme)) {
			auto playlist = plugin->open_uri(uri, mutex);
			if (playlist)
				return playlist;

			tried[i] = true;
		}
	}

	return nullptr;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_uri_suffix(const char *uri, Mutex &mutex,
			      const bool *tried)
{
	assert(uri != nullptr);

	const auto suffix = uri_get_suffix(uri);
	if (suffix.empty())
		return nullptr;

	for (unsigned i = 0; playlist_plugins[i] != nullptr; ++i) {
		const auto *plugin = playlist_plugins[i];

		if (playlist_plugins_enabled[i] && !tried[i] &&
		    plugin->open_uri != nullptr &&
		    plugin->SupportsSuffix(suffix)) {
			auto playlist = plugin->open_uri(uri, mutex);
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_list_open_uri(const char *uri, Mutex &mutex)
{
	/** this array tracks which plugins have already been tried by
	    playlist_list_open_uri_scheme() */
	bool tried[n_playlist_plugins]{};

	assert(uri != nullptr);

	auto playlist = playlist_list_open_uri_scheme(uri, mutex, tried);
	if (playlist == nullptr)
		playlist = playlist_list_open_uri_suffix(uri, mutex,
							 tried);

	return playlist;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_stream_mime2(InputStreamPtr &&is, StringView mime)
{
	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->SupportsMimeType(mime)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			try {
				is->LockRewind();
			} catch (...) {
			}

			auto playlist = plugin->open_stream(std::move(is));
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

static std::unique_ptr<SongEnumerator>
playlist_list_open_stream_mime(InputStreamPtr &&is, std::string_view mime)
{
	/* probe only the portion before the semicolon*/
	return playlist_list_open_stream_mime2(std::move(is),
					       mime);
}

std::unique_ptr<SongEnumerator>
playlist_list_open_stream_suffix(InputStreamPtr &&is, std::string_view suffix)
{
	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->open_stream != nullptr &&
		    plugin->SupportsSuffix(suffix)) {
			/* rewind the stream, so each plugin gets a
			   fresh start */
			try {
				is->LockRewind();
			} catch (...) {
			}

			auto playlist = plugin->open_stream(std::move(is));
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_list_open_stream(InputStreamPtr &&is, const char *uri)
{
	assert(is->IsReady());

	const char *const mime = is->GetMimeType();
	if (mime != nullptr) {
		auto playlist = playlist_list_open_stream_mime(std::move(is),
							       GetMimeTypeBase(mime));
		if (playlist != nullptr)
			return playlist;
	}

	if (uri != nullptr) {
		const auto suffix = uri_get_suffix(uri);
		if (!suffix.empty()) {
			auto playlist = playlist_list_open_stream_suffix(std::move(is),
									 suffix);
			if (playlist != nullptr)
				return playlist;
		}
	}

	return nullptr;
}

const PlaylistPlugin *
FindPlaylistPluginBySuffix(std::string_view suffix) noexcept
{
	playlist_plugins_for_each_enabled(plugin) {
		if (plugin->SupportsSuffix(suffix))
			return plugin;
	}

	return nullptr;
}
