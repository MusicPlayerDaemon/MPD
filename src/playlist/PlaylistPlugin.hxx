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

#ifndef MPD_PLAYLIST_PLUGIN_HXX
#define MPD_PLAYLIST_PLUGIN_HXX

#include "input/Ptr.hxx"
#include "thread/Mutex.hxx"
#include "util/Compiler.h"

struct ConfigBlock;
struct Tag;
struct StringView;
class SongEnumerator;

struct PlaylistPlugin {
	const char *name;

	/**
	 * Initialize the plugin.  Optional method.
	 *
	 * @param block a configuration block for this plugin (may be
	 * empty if none is configured)
	 * @return true if the plugin was initialized successfully,
	 * false if the plugin is not available
	 */
	bool (*init)(const ConfigBlock &block) = nullptr;

	/**
	 * Deinitialize a plugin which was initialized successfully.
	 * Optional method.
	 */
	void (*finish)() = nullptr;

	/**
	 * Opens the playlist on the specified URI.  This URI has
	 * either matched one of the schemes or one of the suffixes.
	 */
	std::unique_ptr<SongEnumerator> (*open_uri)(const char *uri,
						    Mutex &mutex) = nullptr;

	/**
	 * Opens the playlist in the specified input stream.  It has
	 * either matched one of the suffixes or one of the MIME
	 * types.
	 *
	 * @parm is the input stream; the pointer will not be
	 * invalidated when the function returns nullptr
	 */
	std::unique_ptr<SongEnumerator> (*open_stream)(InputStreamPtr &&is) = nullptr;

	const char *const*schemes = nullptr;
	const char *const*suffixes = nullptr;
	const char *const*mime_types = nullptr;

	/**
	 * If true, then playlists of this type are shown in the
	 * database as folders.
	 */
	bool as_folder = false;

	constexpr PlaylistPlugin(const char *_name,
				 std::unique_ptr<SongEnumerator> (*_open_uri)(const char *uri,
									      Mutex &mutex)) noexcept
		:name(_name), open_uri(_open_uri) {}

	constexpr PlaylistPlugin(const char *_name,
				 std::unique_ptr<SongEnumerator> (*_open_stream)(InputStreamPtr &&is)) noexcept
		:name(_name), open_stream(_open_stream) {}

	constexpr auto WithInit(bool (*_init)(const ConfigBlock &block),
				void (*_finish)() noexcept = nullptr) const noexcept {
		auto copy = *this;
		copy.init = _init;
		copy.finish = _finish;
		return copy;
	}

	constexpr auto WithSchemes(const char *const*_schemes) const noexcept {
		auto copy = *this;
		copy.schemes = _schemes;
		return copy;
	}

	constexpr auto WithSuffixes(const char *const*_suffixes) const noexcept {
		auto copy = *this;
		copy.suffixes = _suffixes;
		return copy;
	}

	constexpr auto WithMimeTypes(const char *const*_mime_types) const noexcept {
		auto copy = *this;
		copy.mime_types = _mime_types;
		return copy;
	}

	constexpr auto WithAsFolder(bool value=true) const noexcept {
		auto copy = *this;
		copy.as_folder = value;
		return copy;
	}

	/**
	 * Does the plugin announce the specified URI scheme?
	 */
	gcc_pure gcc_nonnull_all
	bool SupportsScheme(StringView scheme) const noexcept;

	/**
	 * Does the plugin announce the specified file name suffix?
	 */
	gcc_pure gcc_nonnull_all
	bool SupportsSuffix(StringView suffix) const noexcept;

	/**
	 * Does the plugin announce the specified MIME type?
	 */
	gcc_pure gcc_nonnull_all
	bool SupportsMimeType(StringView mime_type) const noexcept;
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
playlist_plugin_init(const PlaylistPlugin *plugin,
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
playlist_plugin_finish(const PlaylistPlugin *plugin) noexcept
{
	if (plugin->finish != nullptr)
		plugin->finish();
}

#endif
