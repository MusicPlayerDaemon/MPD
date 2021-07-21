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

#ifndef MPD_REMOTE_TAG_SCANNER_HXX
#define MPD_REMOTE_TAG_SCANNER_HXX

#include <exception>

struct Tag;

/**
 * Handler for the #RemoteTagScanner result.  It will call one of the
 * methods upon completion.  Must be thread-safe.
 */
class RemoteTagHandler {
public:
	/**
	 * Called on success.
	 */
	virtual void OnRemoteTag(Tag &&tag) noexcept = 0;

	/**
	 * Called on error.
	 */
	virtual void OnRemoteTagError(std::exception_ptr e) noexcept = 0;
};

/**
 * This class can load tags of a remote file.  It is created by
 * InputPlugin::scan_tags(), and the #RemoteTagHandler will be called
 * upon completion.
 *
 * To start the operation, call Start().
 *
 * You can cancel the operation at any time by destructing this
 * object; after successful cancellation, the handler will not be
 * invoked, though it cannot be guaranteed that the handler is not
 * already being called in another thread.
 */
class RemoteTagScanner {
public:
	virtual ~RemoteTagScanner() noexcept = default;
	virtual void Start() = 0;
};

#endif
