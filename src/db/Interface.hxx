/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_DATABASE_INTERFACE_HXX
#define MPD_DATABASE_INTERFACE_HXX

#include "Visitor.hxx"
#include "tag/Type.h"
#include "util/Compiler.h"

#include <chrono>

struct DatabasePlugin;
struct DatabaseStats;
struct DatabaseSelection;
struct LightSong;
class TagMask;

class Database {
	const DatabasePlugin &plugin;

public:
	Database(const DatabasePlugin &_plugin)
		:plugin(_plugin) {}

	/**
	 * Free instance data.
         */
	virtual ~Database() {}

	const DatabasePlugin &GetPlugin() const {
		return plugin;
	}

	bool IsPlugin(const DatabasePlugin &other) const {
		return &plugin == &other;
	}

	/**
         * Open the database.  Read it into memory if applicable.
	 *
	 * Throws #DatabaseError or std::runtime_error on error.
	 */
	virtual void Open() {
	}

	/**
         * Close the database, free allocated memory.
	 */
	virtual void Close() {}

	/**
         * Look up a song (including tag data) in the database.  When
         * you don't need this anymore, call ReturnSong().
	 *
	 * Throws std::runtime_error (or its derivative
	 * #DatabaseError) on error.  "Not found" is an error that
	 * throws DatabaseErrorCode::NOT_FOUND.
	 *
	 * @param uri_utf8 the URI of the song within the music
	 * directory (UTF-8)
	 * @return a pointer that must be released with ReturnSong()
	 */
	virtual const LightSong *GetSong(const char *uri_utf8) const = 0;

	/**
	 * Mark the song object as "unused".  Call this on objects
	 * returned by GetSong().
	 */
	virtual void ReturnSong(const LightSong *song) const = 0;

	/**
	 * Visit the selected entities.
	 */
	virtual void Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist) const = 0;

	void Visit(const DatabaseSelection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song) const {
		Visit(selection, visit_directory, visit_song, VisitPlaylist());
	}

	void Visit(const DatabaseSelection &selection,
		   VisitSong visit_song) const {
		return Visit(selection, VisitDirectory(), visit_song);
	}

	/**
	 * Visit all unique tag values.
	 */
	virtual void VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type, TagMask group_mask,
				     VisitTag visit_tag) const = 0;

	virtual DatabaseStats GetStats(const DatabaseSelection &selection) const = 0;

	/**
	 * Update the database.
	 *
	 * Throws #std::runtime_error on error.
	 *
	 * @return the job id or 0 if not implemented
	 */
	virtual unsigned Update(gcc_unused const char *uri_utf8,
				gcc_unused bool discard) {
		/* not implemented: return 0 */
		return 0;
	}

	/**
	 * Returns the time stamp of the last database update.
	 * Returns a negative value if that is not not known/available.
	 */
	gcc_pure
	virtual std::chrono::system_clock::time_point GetUpdateStamp() const noexcept = 0;
};

#endif
