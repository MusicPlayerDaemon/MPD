// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
