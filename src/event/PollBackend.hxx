// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef EVENT_POLL_BACKEND_HXX
#define EVENT_POLL_BACKEND_HXX

#include "PollResultGeneric.hxx"

#include <cstddef>
#include <vector>
#include <unordered_map>

#include <sys/poll.h>

class PollBackend
{
	struct Item
	{
		std::size_t index;
		void *obj;

		constexpr Item(std::size_t _index, void *_obj) noexcept
			:index(_index), obj(_obj) {}

		Item(const Item &) = delete;
		Item &operator=(const Item &) = delete;
	};

	std::vector<pollfd> poll_events;
	std::unordered_map<int, Item> items;

	PollBackend(PollBackend &) = delete;
	PollBackend &operator=(PollBackend &) = delete;
public:
	PollBackend() noexcept;
	~PollBackend() noexcept;

	PollResultGeneric ReadEvents(int timeout_ms) noexcept;
	bool Add(int fd, unsigned events, void *obj) noexcept;
	bool Modify(int fd, unsigned events, void *obj) noexcept;
	bool Remove(int fd) noexcept;
	bool Abandon(int fd) noexcept {
		return Remove(fd);
	}
};

#endif
