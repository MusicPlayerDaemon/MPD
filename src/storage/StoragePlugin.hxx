// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project


#pragma once

#include <memory>
#include <string_view>

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
					       std::string_view uri);

	/**
	 * Does using a storage of this kind have side effects on the
	 * host (e.g. mounting a filesystem)?  Then only clients with
	 * #PERMISSION_ADMIN may create one (e.g. with "listfiles").
	 */
	bool admin_only = false;

	[[gnu::pure]]
	bool SupportsUri(std::string_view uri) const noexcept;
};
