// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_AVAHI_POLL_HXX
#define MPD_AVAHI_POLL_HXX

#include <avahi-common/watch.h>

class EventLoop;

namespace Avahi {

class Poll final : public AvahiPoll {
	EventLoop &event_loop;

public:
	explicit Poll(EventLoop &_loop) noexcept;

	Poll(const Poll &) = delete;
	Poll &operator=(const Poll &) = delete;

	EventLoop &GetEventLoop() const noexcept {
		return event_loop;
	}

private:
	static AvahiWatch *WatchNew(const AvahiPoll *api, int fd,
				    AvahiWatchEvent event,
				    AvahiWatchCallback callback,
				    void *userdata) noexcept;

	static AvahiTimeout *TimeoutNew(const AvahiPoll *api,
					const struct timeval *tv,
					AvahiTimeoutCallback callback,
					void *userdata) noexcept;
};

} // namespace Avahi

#endif
