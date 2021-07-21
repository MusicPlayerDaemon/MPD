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

#ifndef MPD_UPDATE_REMOVE_HXX
#define MPD_UPDATE_REMOVE_HXX

#include "event/InjectEvent.hxx"
#include "thread/Mutex.hxx"

#include <forward_list>
#include <string>

class DatabaseListener;

/**
 * This class handles #Song removal.  It defers the action to the main
 * thread to ensure that all references to the #Song are gone.
 */
class UpdateRemoveService final {
	DatabaseListener &listener;

	Mutex mutex;

	std::forward_list<std::string> uris;

	InjectEvent defer;

public:
	UpdateRemoveService(EventLoop &_loop, DatabaseListener &_listener)
		:listener(_listener),
		 defer(_loop, BIND_THIS_METHOD(RunDeferred)) {}

	/**
	 * Sends a signal to the main thread which will in turn remove
	 * the song: from the sticker database and from the playlist.
	 * This serialized access is implemented to avoid excessive
	 * locking.
	 */
	void Remove(std::string &&uri);

private:
	/* InjectEvent callback */
	void RunDeferred() noexcept;
};

#endif
