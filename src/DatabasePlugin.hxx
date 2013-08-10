/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

/** \file
 *
 * This header declares the db_plugin class.  It describes a
 * plugin API for databases of song metadata.
 */

#ifndef MPD_DATABASE_PLUGIN_HXX
#define MPD_DATABASE_PLUGIN_HXX

#include "DatabaseVisitor.hxx"
#include "TagType.h"
#include "gcc.h"

struct config_param;
struct DatabaseSelection;
struct db_visitor;
struct Song;
class Error;

struct DatabaseStats {
	/**
	 * Number of songs.
	 */
	unsigned song_count;

	/**
	 * Total duration of all songs (in seconds).
	 */
	unsigned long total_duration;

	/**
	 * Number of distinct artist names.
	 */
	unsigned artist_count;

	/**
	 * Number of distinct album names.
	 */
	unsigned album_count;

	void Clear() {
		song_count = 0;
		total_duration = 0;
		artist_count = album_count = 0;
	}
};

class Database {
public:
	/**
	 * Free instance data.
         */
	virtual ~Database() {}

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
	virtual Song *GetSong(const char *uri_utf8,
			      Error &error) const = 0;

	/**
	 * Mark the song object as "unused".  Call this on objects
	 * returned by GetSong().
	 */
	virtual void ReturnSong(Song *song) const = 0;

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
				     enum tag_type tag_type,
				     VisitString visit_string,
				     Error &error) const = 0;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const = 0;
};

struct DatabasePlugin {
	const char *name;

	/**
	 * Allocates and configures a database.
	 */
	Database *(*create)(const config_param &param,
			    Error &error);
};

#endif
