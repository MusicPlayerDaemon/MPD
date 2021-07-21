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

#ifndef MPD_DATABASE_VISITOR_HELPER_HXX
#define MPD_DATABASE_VISITOR_HELPER_HXX

#include "Visitor.hxx"
#include "Selection.hxx"

#include <vector>

class DetachedSong;

/**
 * This class helps implementing Database::Visit() by emulating
 * #DatabaseSelection features that the #Database implementation
 * doesn't have, e.g. filtering, sorting and window.
 *
 * To use this class, construct it, passing unsupported features and
 * the visitor callback to the constructor; before leaving Visit(),
 * call Commit() (unless an error has occurred).
 */
class DatabaseVisitorHelper {
	const DatabaseSelection selection;

	/**
	 * If the plugin can't sort, then this container will collect
	 * all songs, sort them and report them to the visitor in
	 * Commit().
	 */
	std::vector<DetachedSong> songs;

	VisitSong original_visit_song;

	/**
	 * Used to emulate the "window".
	 */
	unsigned counter = 0;

public:
	/**
	 * @param selection a #DatabaseSelection instance with only
	 * features enabled which shall be emulated by this class
	 * @param visit_song the callback function passed to
	 * Database::Visit(); may be replaced by this class
	 */
	DatabaseVisitorHelper(DatabaseSelection selection,
			      VisitSong &visit_song) noexcept;
	~DatabaseVisitorHelper() noexcept;

	void Commit();
};

#endif
