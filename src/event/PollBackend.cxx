// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PollBackend.hxx"

#include <cassert>

PollBackend::PollBackend() noexcept = default;
PollBackend::~PollBackend() noexcept = default;

static constexpr auto
MakePollfd(int fd, short events) noexcept
{
	struct pollfd pfd{};
	pfd.fd = fd;
	pfd.events = events;
	return pfd;
}

bool
PollBackend::Add(int fd, unsigned events, void *obj) noexcept
{
	assert(items.find(fd) == items.end());

	const std::size_t index = poll_events.size();
	poll_events.push_back(MakePollfd(fd, events));

	items.emplace(std::piecewise_construct,
		      std::forward_as_tuple(fd),
		      std::forward_as_tuple(index, obj));
	return true;
}

bool
PollBackend::Modify(int fd, unsigned events, void *obj) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	auto &item = item_iter->second;
	item.obj = obj;
	auto &e = poll_events[item.index];
	e.events = events;
	e.revents &= events;
	return true;
}

bool
PollBackend::Remove(int fd) noexcept
{
	auto item_iter = items.find(fd);
	assert(item_iter != items.end());
	const auto &item = item_iter->second;
	std::size_t index = item.index;
	std::size_t last_index = poll_events.size() - 1;
	if (index != last_index) {
		std::swap(poll_events[index], poll_events[last_index]);
		items.find(poll_events[index].fd)->second.index = index;
	}
	poll_events.pop_back();
	items.erase(item_iter);
	return true;
}

PollResultGeneric
PollBackend::ReadEvents(int timeout_ms) noexcept
{
	int n = poll(poll_events.empty() ? nullptr : &poll_events[0],
		     poll_events.size(), timeout_ms);

	PollResultGeneric result;
	for (std::size_t i = 0; n > 0 && i < poll_events.size(); ++i) {
		const auto &e = poll_events[i];
		if (e.revents != 0) {
			auto it = items.find(e.fd);
			assert(it != items.end());
			assert(it->second.index == i);

			result.Add(e.revents, it->second.obj);
			--n;
		}
	}

	return result;
}
