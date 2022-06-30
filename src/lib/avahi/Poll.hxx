/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
