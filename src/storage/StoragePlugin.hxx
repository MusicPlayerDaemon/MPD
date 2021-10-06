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
