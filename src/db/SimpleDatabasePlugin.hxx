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

#ifndef MPD_SIMPLE_DATABASE_PLUGIN_HXX
#define MPD_SIMPLE_DATABASE_PLUGIN_HXX

#include "DatabasePlugin.hxx"
#include "fs/AllocatedPath.hxx"
#include "Compiler.h"

#include <cassert>

#include <time.h>

struct Directory;

class SimpleDatabase : public Database {
	AllocatedPath path;
	std::string path_utf8;

	Directory *root;

	time_t mtime;

#ifndef NDEBUG
	unsigned borrowed_song_count;
#endif

	SimpleDatabase()
		:path(AllocatedPath::Null()) {}

public:
	gcc_pure
	Directory *GetRoot() {
		assert(root != NULL);

		return root;
	}

	bool Save(Error &error);

	gcc_pure
	time_t GetLastModified() const {
		return mtime;
	}

	static Database *Create(const config_param &param,
				Error &error);

	virtual bool Open(Error &error) override;
	virtual void Close() override;

	virtual Song *GetSong(const char *uri_utf8,
			      Error &error) const override;
	virtual void ReturnSong(Song *song) const;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type,
				     VisitString visit_string,
				     Error &error) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const override;

protected:
	bool Configure(const config_param &param, Error &error);

	gcc_pure
	bool Check(Error &error) const;

	bool Load(Error &error);

	gcc_pure
	const Directory *LookupDirectory(const char *uri) const;
};

extern const DatabasePlugin simple_db_plugin;

#endif
