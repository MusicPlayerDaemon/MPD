// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
