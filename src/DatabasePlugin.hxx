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
#include "gcc.h"

struct config_param;
struct db_selection;
struct db_visitor;

class Database {
public:
	/**
	 * Free instance data.
         */
	virtual ~Database() {}

	/**
         * Open the database.  Read it into memory if applicable.
	 */
	virtual bool Open(gcc_unused GError **error_r) {
		return true;
	}

	/**
         * Close the database, free allocated memory.
	 */
	virtual void Close() {}

	/**
         * Look up a song (including tag data) in the database.
	 *
	 * @param uri_utf8 the URI of the song within the music
	 * directory (UTF-8)
	 */
	virtual struct song *GetSong(const char *uri_utf8,
				     GError **error_r) const = 0;

	/**
	 * Visit the selected entities.
	 */
	virtual bool Visit(const db_selection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   GError **error_r) const = 0;

	bool Visit(const db_selection &selection,
		   VisitDirectory visit_directory,
		   VisitSong visit_song,
		   GError **error_r) const {
		return Visit(selection, visit_directory, visit_song,
			     VisitPlaylist(), error_r);
	}

	bool Visit(const db_selection &selection, VisitSong visit_song,
		   GError **error_r) const {
		return Visit(selection, VisitDirectory(), visit_song, error_r);
	}
};

struct DatabasePlugin {
	const char *name;

	/**
	 * Allocates and configures a database.
	 */
	Database *(*create)(const struct config_param *param,
			    GError **error_r);
};

#endif
