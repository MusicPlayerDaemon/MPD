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

#ifndef MPD_CLIENT_LIST_HXX
#define MPD_CLIENT_LIST_HXX

#include "Client.hxx"

#include <boost/intrusive/list.hpp>

class ClientList {
	using List =
		boost::intrusive::list<Client,
				       boost::intrusive::constant_time_size<true>>;

	const unsigned max_size;

	List list;

public:
	explicit ClientList(unsigned _max_size) noexcept
		:max_size(_max_size) {}

	~ClientList() noexcept;

	auto begin() noexcept {
		return list.begin();
	}

	auto end() noexcept {
		return list.end();
	}

	bool IsFull() const noexcept {
		return list.size() >= max_size;
	}

	void Add(Client &client) noexcept {
		list.push_front(client);
	}

	void Remove(Client &client) noexcept;
};

#endif
