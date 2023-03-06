// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef EVENT_EPOLL_EVENTS_HXX
#define EVENT_EPOLL_EVENTS_HXX

#include <sys/epoll.h>

struct EpollEvents {
	static constexpr unsigned READ = EPOLLIN;
	static constexpr unsigned WRITE = EPOLLOUT;
	static constexpr unsigned ERROR = EPOLLERR;
	static constexpr unsigned HANGUP = EPOLLHUP;
};

#endif
