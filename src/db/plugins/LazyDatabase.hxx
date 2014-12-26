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

#ifndef MPD_LAZY_DATABASE_PLUGIN_HXX
#define MPD_LAZY_DATABASE_PLUGIN_HXX

#include "db/Interface.hxx"
#include "Compiler.h"

/**
 * A wrapper for a #Database object that gets opened on the first
 * database access.  This works around daemonization problems with
 * some plugins.
 */
class LazyDatabase final : public Database {
	Database *const db;

	mutable bool open;

public:
	gcc_nonnull_all
	LazyDatabase(Database *_db);

	virtual ~LazyDatabase();

	virtual void Close() override;

	virtual const LightSong *GetSong(const char *uri_utf8,
					 Error &error) const override;
	void ReturnSong(const LightSong *song) const override;

	virtual bool Visit(const DatabaseSelection &selection,
			   VisitDirectory visit_directory,
			   VisitSong visit_song,
			   VisitPlaylist visit_playlist,
			   Error &error) const override;

	virtual bool VisitUniqueTags(const DatabaseSelection &selection,
				     TagType tag_type, uint32_t group_mask,
				     VisitTag visit_tag,
				     Error &error) const override;

	virtual bool GetStats(const DatabaseSelection &selection,
			      DatabaseStats &stats,
			      Error &error) const override;

	virtual time_t GetUpdateStamp() const override;

private:
	bool EnsureOpen(Error &error) const;
};

#endif
