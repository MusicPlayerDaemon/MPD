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

#ifndef MPD_CLIENT_LIST_HXX
#define MPD_CLIENT_LIST_HXX

#include "Client.hxx"

class Client;

class ClientList {
	typedef boost::intrusive::list<Client,
				       boost::intrusive::constant_time_size<true>> List;

	const unsigned max_size;

	List list;

public:
	ClientList(unsigned _max_size)
		:max_size(_max_size) {}
	~ClientList() {
		CloseAll();
	}

	List::iterator begin() {
		return list.begin();
	}

	List::iterator end() {
		return list.end();
	}

	bool IsFull() const {
		return list.size() >= max_size;
	}

	void Add(Client &client) {
		list.push_front(client);
	}

	void Remove(Client &client);

	void CloseAll();

	void IdleAdd(unsigned flags);
};

#endif
