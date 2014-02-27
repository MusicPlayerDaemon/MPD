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
#include "DatabaseQueue.hxx"
#include "DatabaseSong.hxx"
#include "Interface.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "DetachedSong.hxx"

#include <functional>

static bool
AddToQueue(Partition &partition, const LightSong &song, Error &error)
{
	const Storage &storage = *partition.instance.storage;
	unsigned id =
		partition.playlist.AppendSong(partition.pc,
					      DatabaseDetachSong(storage,
								 song),
					      error);
	return id != 0;
}

bool
AddFromDatabase(Partition &partition, const DatabaseSelection &selection,
		Error &error)
{
	const Database *db = partition.instance.GetDatabase(error);
	if (db == nullptr)
		return false;

	using namespace std::placeholders;
	const auto f = std::bind(AddToQueue, std::ref(partition), _1, _2);
	return db->Visit(selection, f, error);
}
