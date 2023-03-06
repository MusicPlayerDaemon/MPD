// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CLIENT_LIST_HXX
#define MPD_CLIENT_LIST_HXX

#include "Client.hxx"
#include "util/IntrusiveList.hxx"

class ClientList {
	using List =
		IntrusiveList<Client, IntrusiveListMemberHookTraits<&Client::list_siblings>, true>;

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
