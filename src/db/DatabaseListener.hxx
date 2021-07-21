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
