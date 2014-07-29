/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "tag/TagType.h"
#include "Compiler.h"

#include <time.h>
#include <stdint.h>

struct DatabasePlugin;
struct DatabaseStats;
struct DatabaseSelection;
struct LightSong;
class Error;

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
	 */
	virtual bool Open(gcc_unused Error &error) {
		return true;
	}

	/**
         * Close the database, free allocated memory.
	 */
	virtual void Close() {}

	/**
         * Look up a song (including tag data) in the database.  When
         * you don't need this anymore, call ReturnSong().
	 *
	 * @param uri_utf8 the URI of the song within the music
	 * directory (UTF-8)
	 */
	virtual const LightSong *GetSong(const char *uri_utf8,
					 Error &error) const = 0;

	/**
	 * Mark the song object as "unused".  Call this on objects
	 * returned by GetSong().
	 */
	virtual void ReturnSong(const LightSong *song) const = 0;

	/**
	 * Visit the selected entities.
	 */
	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const = 0;

	bool Visit(const DatabaseSelection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   Error &error) const {
		return Visit(selection, visit_directory, visit_song,
			     VisitPlaylist(), error);
	}

	bool Visit(const DatabaseSelection &selection, VisitSong visit_song,
		   Error &error) const {
		return Visit(selection, VisitDirectory(), visit_song, error);
	}

	/**
	 * Visit all unique tag values.
	 */
	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type, uint32_t group_mask,
				     VisitTag visit_tag,
				     Error &error) const = 0;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const = 0;

	/**
	 * Update the database.  Returns the job id on success, 0 on
	 * error (with #Error set) and 0 if not implemented (#Error
	 * not set).
	 */
	virtual unsigned Update(gcc_unused const char *uri_utf8,
				gcc_unused bool discard,
				gcc_unused Error &error) {
		/* not implemented: return 0 and don't set an Error */
		return 0;
	}

	/**
	 * Returns the time stamp of the last database update.
	 * Returns 0 if that is not not known/available.
	 */
	gcc_pure
	virtual time_t GetUpdateStamp() const = 0;
};

#endif
