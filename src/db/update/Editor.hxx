// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
