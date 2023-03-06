// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_QUEUE_LISTENER_HXX
#define MPD_QUEUE_LISTENER_HXX

class QueueListener {
public:
	/**
	 * Called after the queue has been modified.
	 */
	virtual void OnQueueModified() noexcept = 0;

	/**
	 * Called after a playback options have been changed.
	 */
	virtual void OnQueueOptionsChanged() noexcept = 0;

	/**
	 * Called after the player has started playing a new song.
	 * This gets called by playlist::SyncWithPlayer() after it has
	 * been notified by the player thread.
	 */
	virtual void OnQueueSongStarted() noexcept = 0;
};

#endif
