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
#include "DatabaseSong.hxx"
#include "LightSong.hxx"
#include "Interface.hxx"
#include "DetachedSong.hxx"
#include "storage/StorageInterface.hxx"

#include <assert.h>

DetachedSong
DatabaseDetachSong(const Storage &storage, const LightSong &song)
{
	DetachedSong detached(song);
	assert(detached.IsInDatabase());

	if (!detached.HasRealURI()) {
		const auto uri = song.GetURI();
		detached.SetRealURI(storage.MapUTF8(uri.c_str()));
	}

	return detached;
}

DetachedSong *
DatabaseDetachSong(const Database &db, const Storage &storage, const char *uri,
		   Error &error)
{
	const LightSong *tmp = db.GetSong(uri, error);
	if (tmp == nullptr)
		return nullptr;

	DetachedSong *song = new DetachedSong(DatabaseDetachSong(storage,
								 *tmp));
	db.ReturnSong(tmp);
	return song;
}
