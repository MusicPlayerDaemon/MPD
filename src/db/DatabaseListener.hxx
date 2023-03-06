// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_CLIENT_HXX
#define MPD_DATABASE_CLIENT_HXX

struct LightSong;

/**
 * An object that listens to events from the #Database.
 *
 * @see #Instance
 */
class DatabaseListener {
public:
	/**
	 * The database has been modified.  This must be called in the
	 * thread that has created the #Database instance and that
	 * runs the #EventLoop.
	 */
	virtual void OnDatabaseModified() noexcept = 0;

	/**
	 * During database update, a song is about to be removed from
	 * the database because the file has disappeared.
	 */
	virtual void OnDatabaseSongRemoved(const char *uri) noexcept = 0;
};

#endif
