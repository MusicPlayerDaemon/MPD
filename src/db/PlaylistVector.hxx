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

#ifndef MPD_PLAYLIST_VECTOR_HXX
#define MPD_PLAYLIST_VECTOR_HXX

#include "db/PlaylistInfo.hxx"

#include <list>
#include <string_view>

class PlaylistVector : protected std::list<PlaylistInfo> {
protected:
	/**
	 * Caller must lock the #db_mutex.
	 */
	[[gnu::pure]]
	iterator find(std::string_view name) noexcept;

public:
	using std::list<PlaylistInfo>::empty;
	using std::list<PlaylistInfo>::begin;
	using std::list<PlaylistInfo>::end;
	using std::list<PlaylistInfo>::push_back;
	using std::list<PlaylistInfo>::erase;

	/**
	 * Caller must lock the #db_mutex.
	 *
	 * @return true if the vector or one of its items was modified
	 */
	bool UpdateOrInsert(PlaylistInfo &&pi) noexcept;

	/**
	 * Caller must lock the #db_mutex.
	 */
	bool erase(std::string_view name) noexcept;
};

#endif /* SONGVEC_H */
