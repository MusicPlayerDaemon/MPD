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

#ifndef MPD_INPUT_PLUGIN_HXX
#define MPD_INPUT_PLUGIN_HXX

#include "Ptr.hxx"
#include "thread/Mutex.hxx"

#include <cassert>
#include <set>
#include <string>

struct ConfigBlock;
class EventLoop;
class RemoteTagScanner;
class RemoteTagHandler;

struct InputPlugin {
	const char *name;

	/**
	 * A nullptr-terminated list of URI prefixes handled by this
	 * plugin.  This is usually a string in the form "scheme://".
	 */
	const char *const*prefixes;

	/**
	 * Global initialization.  This method is called when MPD starts.
	 *
	 * Throws #PluginUnavailable if the plugin is not available
	 * and shall be disabled.
	 *
	 * Throws std::runtime_error on (fatal) error.
	 */
	void (*init)(EventLoop &event_loop, const ConfigBlock &block);

	/**
	 * Global deinitialization.  Called once before MPD shuts
	 * down (only if init() has returned true).
	 */
	void (*finish)() noexcept;

	/**
	 * Attempt to open the given URI.  Returns nullptr if the
	 * plugin does not support this URI.
	 *
	 * Throws std::runtime_error on error.
	 */
	InputStreamPtr (*open)(const char *uri, Mutex &mutex);

	/**
	 * return a set of supported protocols
	 */
	std::set<std::string> (*protocols)() noexcept;

	/**
	 * Prepare a #RemoteTagScanner.  The operation must be started
	 * using RemoteTagScanner::Start().  Returns nullptr if the
	 * plugin does not support this URI.
	 *
	 * Throws on error.
	 *
	 * @return nullptr if the given URI is not supported.
	 */
	std::unique_ptr<RemoteTagScanner> (*scan_tags)(const char *uri,
						       RemoteTagHandler &handler) = nullptr;

	[[gnu::pure]]
	bool SupportsUri(const char *uri) const noexcept;

	template<typename F>
	void ForeachSupportedUri(F lambda) const noexcept {
		assert(prefixes || protocols);

		if (prefixes != nullptr) {
			for (auto schema = prefixes; *schema != nullptr; ++schema) {
				lambda(*schema);
			}
		}
		if (protocols != nullptr) {
			for (auto schema : protocols()) {
				lambda(schema.c_str());
			}
		}
	}
};

[[gnu::pure]]
bool
protocol_is_whitelisted(const char *proto) noexcept;

#endif
