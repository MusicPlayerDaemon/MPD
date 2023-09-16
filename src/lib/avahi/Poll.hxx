// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

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
