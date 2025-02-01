// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "system/EpollFD.hxx"

#include <array>
#include <algorithm>

class EpollBackendResult
{
	friend class EpollBackend;

	std::array<epoll_event, 16> events;
	size_t n_events = 0;

public:
	size_t GetSize() const noexcept {
		return n_events;
	}

	unsigned GetEvents(size_t i) const noexcept {
		return events[i].events;
	}

	void *GetObject(size_t i) const noexcept {
		return events[i].data.ptr;
	}
};

class EpollBackend
{
	EpollFD epoll;

	EpollBackend(EpollBackend &) = delete;
	EpollBackend &operator=(EpollBackend &) = delete;
public:
	EpollBackend() = default;

	FileDescriptor GetFileDescriptor() const noexcept {
		return epoll.GetFileDescriptor();
	}

	auto ReadEvents(int timeout_ms) noexcept {
		EpollBackendResult result;
		int ret = epoll.Wait(result.events.data(), result.events.size(),
				     timeout_ms);
		result.n_events = std::max(0, ret);
		return result;
	}

	bool Add(int fd, unsigned events, void *obj) noexcept {
		return epoll.Add(fd, events, obj);
	}

	bool Modify(int fd, unsigned events, void *obj) noexcept {
		return epoll.Modify(fd, events, obj);
	}

	bool Remove(int fd) noexcept {
		return epoll.Remove(fd);
	}

	bool Abandon([[maybe_unused]] int fd) noexcept {
		// Nothing to do in this implementation.
		// Closed descriptors are automatically unregistered.
		return true;
	}
};
