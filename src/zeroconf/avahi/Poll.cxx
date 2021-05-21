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

#include "Poll.hxx"
#include "event/SocketEvent.hxx"
#include "event/FineTimerEvent.hxx"
#include "time/Convert.hxx"

static constexpr unsigned
FromAvahiWatchEvent(AvahiWatchEvent e) noexcept
{
	return (e & AVAHI_WATCH_IN ? SocketEvent::READ : 0) |
		(e & AVAHI_WATCH_OUT ? SocketEvent::WRITE : 0);
}

static constexpr AvahiWatchEvent
ToAvahiWatchEvent(unsigned e) noexcept
{
	return AvahiWatchEvent((e & SocketEvent::READ ? AVAHI_WATCH_IN : 0) |
			       (e & SocketEvent::WRITE ? AVAHI_WATCH_OUT : 0) |
			       (e & SocketEvent::ERROR ? AVAHI_WATCH_ERR : 0) |
			       (e & SocketEvent::HANGUP ? AVAHI_WATCH_HUP : 0));
}

struct AvahiWatch final {
private:
	SocketEvent event;

	const AvahiWatchCallback callback;
	void *const userdata;

	AvahiWatchEvent received = AvahiWatchEvent(0);

public:
	AvahiWatch(EventLoop &_loop,
		   SocketDescriptor _fd, AvahiWatchEvent _event,
		   AvahiWatchCallback _callback, void *_userdata) noexcept
		:event(_loop, BIND_THIS_METHOD(OnSocketReady), _fd),
		 callback(_callback), userdata(_userdata) {
		event.Schedule(FromAvahiWatchEvent(_event));
	}

	static void WatchUpdate(AvahiWatch *w,
				AvahiWatchEvent _event) noexcept {
		w->event.Schedule(FromAvahiWatchEvent(_event));
	}

	static AvahiWatchEvent WatchGetEvents(AvahiWatch *w) noexcept {
		return w->received;
	}

	static void WatchFree(AvahiWatch *w) noexcept {
		delete w;
	}

private:
	void OnSocketReady(unsigned events) noexcept {
		received = ToAvahiWatchEvent(events);
		callback(this, event.GetSocket().Get(), received, userdata);
		received = AvahiWatchEvent(0);
	}
};

struct AvahiTimeout final {
	/* note: cannot use CoarseTimerEvent because libavahi-client
	   sometimes schedules events immediately, and
	   CoarseTimerEvent may delay the timer callback for too
	   long, causing timeouts */
	FineTimerEvent event;

	const AvahiTimeoutCallback callback;
	void *const userdata;

public:
	AvahiTimeout(EventLoop &_loop, const struct timeval *tv,
		     AvahiTimeoutCallback _callback, void *_userdata) noexcept
		:event(_loop, BIND_THIS_METHOD(OnTimeout)),
		 callback(_callback), userdata(_userdata) {
		if (tv != nullptr)
			Schedule(*tv);
	}

	static void TimeoutUpdate(AvahiTimeout *t,
				  const struct timeval *tv) noexcept {
		if (tv != nullptr)
			t->Schedule(*tv);
		else
			t->event.Cancel();
	}

	static void TimeoutFree(AvahiTimeout *t) noexcept {
		delete t;
	}

private:
	[[gnu::pure]]
	Event::Duration AbsoluteToDuration(const struct timeval &tv) noexcept {
		if (tv.tv_sec == 0)
			/* schedule immediately */
			return {};

		struct timeval now;
		if (gettimeofday(&now, nullptr) < 0)
			/* shouldn't ever fail, but if it does, do
			   something reasonable */
			return std::chrono::seconds(1);

		auto d = ToSteadyClockDuration(tv)
			- ToSteadyClockDuration(now);
		if (d.count() < 0)
			return {};

		return d;
	}

	void Schedule(const struct timeval &tv) noexcept {
		event.Schedule(AbsoluteToDuration(tv));
	}

	void OnTimeout() noexcept {
		callback(this, userdata);
	}
};

namespace Avahi {

Poll::Poll(EventLoop &_loop) noexcept
	:event_loop(_loop)
{
	watch_new = WatchNew;
	watch_update = AvahiWatch::WatchUpdate;
	watch_get_events = AvahiWatch::WatchGetEvents;
	watch_free = AvahiWatch::WatchFree;
	timeout_new = TimeoutNew;
	timeout_update = AvahiTimeout::TimeoutUpdate;
	timeout_free = AvahiTimeout::TimeoutFree;
}

AvahiWatch *
Poll::WatchNew(const AvahiPoll *api, int fd, AvahiWatchEvent event,
	       AvahiWatchCallback callback, void *userdata) noexcept
{
	const Poll &poll = *(const Poll *)api;

	return new AvahiWatch(poll.event_loop, SocketDescriptor(fd), event,
			      callback, userdata);
}

AvahiTimeout *
Poll::TimeoutNew(const AvahiPoll *api, const struct timeval *tv,
		 AvahiTimeoutCallback callback, void *userdata) noexcept
{
	const Poll &poll = *(const Poll *)api;

	return new AvahiTimeout(poll.event_loop, tv, callback, userdata);
}

} // namespace Avahi
