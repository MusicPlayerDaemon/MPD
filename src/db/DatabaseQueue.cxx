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

#include "config.h"
#include "DatabaseQueue.hxx"
#include "DatabaseSong.hxx"
#include "Interface.hxx"
#include "Partition.hxx"
#include "Instance.hxx"
#include "DetachedSong.hxx"
#include "tag/Builder.hxx"

#include <functional>

static std::string
get_parent(std::string str)
{
	auto p1 = str.rfind('/');
	if (p1 == std::string::npos) {
		return std::string("Folder");
	}

	auto p2 = str.rfind('/', p1-1);
	if (p2 == std::string::npos) {
		return std::string("Folder");
	}
	return str.substr(p2+1, p1-p2-1);
}

static void
AddToQueue(Partition &partition, const LightSong &song)
{
	const auto *storage = partition.instance.storage;
	auto dsong = DatabaseDetachSong(storage, song);
	const Tag &tag = dsong.GetTag();
	if (!tag.HasType(TAG_ALBUM)) {
		TagBuilder tb(tag);
		tb.AddItem(TAG_ALBUM, get_parent(dsong.GetURI()).c_str());
		dsong.SetTag(std::move(tb.Commit()));
	}

	partition.playlist.AppendSong(partition.pc, std::move(dsong));
}

void
AddFromDatabase(Partition &partition, const DatabaseSelection &selection)
{
	const Database &db = partition.instance.GetDatabaseOrThrow();

	using namespace std::placeholders;
	const auto f = std::bind(AddToQueue, std::ref(partition), _1);
	db.Visit(selection, f);
}
