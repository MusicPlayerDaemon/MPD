/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "PlaylistVector.hxx"
#include "db/DatabaseLock.hxx"

#include <algorithm>
#include <cassert>

PlaylistVector::iterator
PlaylistVector::find(std::string_view name) noexcept
{
	assert(holding_db_lock());

	return std::find_if(begin(), end(),
			    PlaylistInfo::CompareName(name));
}

bool
PlaylistVector::UpdateOrInsert(PlaylistInfo &&pi) noexcept
{
	assert(holding_db_lock());

	auto i = find(pi.name.c_str());
	if (i != end()) {
		if (pi.mtime == i->mtime)
			return false;

		i->mtime = pi.mtime;
	} else
		push_back(std::move(pi));

	return true;
}

bool
PlaylistVector::erase(std::string_view name) noexcept
{
	assert(holding_db_lock());

	auto i = find(name);
	if (i == end())
		return false;

	erase(i);
	return true;
}
