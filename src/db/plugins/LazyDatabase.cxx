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

#include "config.h"
#include "LazyDatabase.hxx"
#include "db/Interface.hxx"

#include <assert.h>

LazyDatabase::LazyDatabase(Database *_db)
	:Database(_db->GetPlugin()), db(_db), open(false) {}

LazyDatabase::~LazyDatabase()
{
	assert(!open);

	delete db;
}

bool
LazyDatabase::EnsureOpen(Error &error) const
{
	if (open)
		return true;

	if (!db->Open(error))
		return false;

	open = true;
	return true;
}

void
LazyDatabase::Close()
{
	if (open) {
		open = false;
		db->Close();
	}
}

const LightSong *
LazyDatabase::GetSong(const char *uri, Error &error) const
{
	return EnsureOpen(error)
		? db->GetSong(uri, error)
		: nullptr;
}

void
LazyDatabase::ReturnSong(const LightSong *song) const
{
	assert(open);

	db->ReturnSong(song);
}

bool
LazyDatabase::Visit(const DatabaseSelection &selection,
		    VisitDirectory visit_directory,
		    VisitSong visit_song,
		    VisitPlaylist visit_playlist,
		    Error &error) const
{
	return EnsureOpen(error) &&
		db->Visit(selection, visit_directory, visit_song,
			  visit_playlist, error);
}

bool
LazyDatabase::VisitUniqueTags(const DatabaseSelection &selection,
			      TagType tag_type, uint32_t group_mask,
			      VisitTag visit_tag,
			      Error &error) const
{
	return EnsureOpen(error) &&
		db->VisitUniqueTags(selection, tag_type, group_mask, visit_tag,
				    error);
}

bool
LazyDatabase::GetStats(const DatabaseSelection &selection,
		       DatabaseStats &stats, Error &error) const
{
	return EnsureOpen(error) && db->GetStats(selection, stats, error);
}

time_t
LazyDatabase::GetUpdateStamp() const
{
	return open ? db->GetUpdateStamp() : 0;
}
