// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
		i->mark = true;
	} else {
		pi.mark = true;
		push_back(std::move(pi));
	}

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
