// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STORAGE_PLUGIN_HXX
#define MPD_STORAGE_PLUGIN_HXX

#include <memory>

class Storage;
class EventLoop;

struct StoragePlugin {
	const char *name;

	/**
	 * A nullptr-terminated list of URI prefixes handled by this
	 * plugin.  This is usually a string in the form "scheme://".
	 */
	const char *const*prefixes;

	/**
	 * Throws #std::runtime_error on error.
	 */
	std::unique_ptr<Storage> (*create_uri)(EventLoop &event_loop,
					       const char *uri);

	[[gnu::pure]]
	bool SupportsUri(const char *uri) const noexcept;
};

#endif
