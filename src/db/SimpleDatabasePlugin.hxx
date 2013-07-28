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
#include "fs/Path.hxx"
#include "gcc.h"

#include <cassert>

#include <time.h>

struct Directory;

class SimpleDatabase : public Database {
	Path path;
	std::string path_utf8;

	Directory *root;

	time_t mtime;

#ifndef NDEBUG
	unsigned borrowed_song_count;
#endif

	SimpleDatabase()
		:path(Path::Null()) {}

public:
	gcc_pure
	Directory *GetRoot() {
		assert(root != NULL);

		return root;
	}

	bool Save(GError **error_r);

	gcc_pure
	time_t GetLastModified() const {
		return mtime;
	}

	static Database *Create(const struct config_param *param,
				GError **error_r);

	virtual bool Open(GError **error_r) override;
	virtual void Close() override;

	virtual Song *GetSong(const char *uri_utf8,
				     GError **error_r) const override;
	virtual void ReturnSong(Song *song) const;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   GError **error_r) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     enum tag_type tag_type,
				     VisitString visit_string,
				     GError **error_r) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      GError **error_r) const override;

protected:
	bool Configure(const struct config_param *param, GError **error_r);

	gcc_pure
	bool Check(GError **error_r) const;

	bool Load(GError **error_r);

	gcc_pure
	const Directory *LookupDirectory(const char *uri) const;
};

extern const DatabasePlugin simple_db_plugin;

#endif
