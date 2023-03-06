// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_SONG_HXX
#define MPD_DATABASE_SONG_HXX

struct LightSong;
class Database;
class Storage;
class DetachedSong;

/**
 * "Detach" the #Song object, i.e. convert it to a #DetachedSong
 * instance.
 */
DetachedSong
DatabaseDetachSong(const Storage *storage, const LightSong &song) noexcept;

/**
 * Look up a song in the database and convert it to a #DetachedSong
 * instance.
 *
 * Throws std::runtime_error on error.
 */
DetachedSong
DatabaseDetachSong(const Database &db, const Storage *storage,
		   const char *uri);

#endif
