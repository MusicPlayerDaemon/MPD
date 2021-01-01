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

#ifndef MPD_UPDATE_DATABASE_HXX
#define MPD_UPDATE_DATABASE_HXX

#include "Remove.hxx"

struct Directory;
struct Song;

class DatabaseEditor final {
	UpdateRemoveService remove;

public:
	DatabaseEditor(EventLoop &_loop, DatabaseListener &_listener)
		:remove(_loop, _listener) {}

	/**
	 * Caller must lock the #db_mutex.
	 */
	void DeleteSong(Directory &parent, Song *song);

	/**
	 * DeleteSong() with automatic locking.
	 */
	void LockDeleteSong(Directory &parent, Song *song);

	/**
	 * Recursively free a directory and all its contents.
	 *
	 * Caller must lock the #db_mutex.
	 */
	void DeleteDirectory(Directory *directory);

	/**
	 * DeleteDirectory() with automatic locking.
	 */
	void LockDeleteDirectory(Directory *directory);

	/**
	 * Caller must NOT lock the #db_mutex.
	 *
	 * @return true if the database was modified
	 */
	bool DeleteNameIn(Directory &parent, std::string_view name);

private:
	void ClearDirectory(Directory &directory);
};

#endif
