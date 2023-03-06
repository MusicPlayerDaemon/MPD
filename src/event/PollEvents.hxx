// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef EVENT_POLL_EVENTS_HXX
#define EVENT_POLL_EVENTS_HXX

#include <sys/poll.h>

struct PollEvents {
	static constexpr unsigned READ = POLLIN;
	static constexpr unsigned WRITE = POLLOUT;
	static constexpr unsigned ERROR = POLLERR;
	static constexpr unsigned HANGUP = POLLHUP;
};

#endif
