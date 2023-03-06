// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_INOTIFY_QUEUE_HXX
#define MPD_INOTIFY_QUEUE_HXX

#include "event/CoarseTimerEvent.hxx"

#include <list>
#include <string>

class UpdateService;

class InotifyQueue final {
	UpdateService &update;

	std::list<std::string> queue;

	CoarseTimerEvent delay_event;

public:
	InotifyQueue(EventLoop &_loop, UpdateService &_update) noexcept
		:update(_update),
		 delay_event(_loop, BIND_THIS_METHOD(OnDelay)) {}

	void Enqueue(const char *uri_utf8) noexcept;

private:
	void OnDelay() noexcept;
};

#endif
