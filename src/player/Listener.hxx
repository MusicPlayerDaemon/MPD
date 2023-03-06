// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYER_LISTENER_HXX
#define MPD_PLAYER_LISTENER_HXX

class PlayerListener {
public:
	/**
	 * A playback error has occurred and
	 * PlayerControl::CheckRethrowError() will provide details.
	 */
	virtual void OnPlayerError() noexcept = 0;

	/**
	 * Some state of the player has changed.  This maps to
	 * #IDLE_PLAYER.
	 */
	virtual void OnPlayerStateChanged() noexcept = 0;

	/**
	 * Some options of the player has changed.  This maps to
	 * #IDLE_OPTIONS.
	 */
	virtual void OnPlayerOptionsChanged() noexcept = 0;

	/**
	 * Must call playlist_sync().
	 */
	virtual void OnPlayerSync() noexcept = 0;

	/**
	 * The current song's tag has changed.
	 */
	virtual void OnPlayerTagModified() noexcept = 0;

	/**
	 * Playback went into border pause.
	 */
	virtual void OnBorderPause() noexcept = 0;
};

#endif
