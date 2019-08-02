/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "AvahiPoll.hxx"
#include "event/SocketMonitor.hxx"
#include "event/TimerEvent.hxx"
#include "time/Convert.hxx"

static unsigned
FromAvahiWatchEvent(AvahiWatchEvent e)
{
	return (e & AVAHI_WATCH_IN ? SocketMonitor::READ : 0) |
		(e & AVAHI_WATCH_OUT ? SocketMonitor::WRITE : 0) |
		(e & AVAHI_WATCH_ERR ? SocketMonitor::ERROR : 0) |
		(e & AVAHI_WATCH_HUP ? SocketMonitor::HANGUP : 0);
}

static AvahiWatchEvent
ToAvahiWatchEvent(unsigned e)
{
	return AvahiWatchEvent((e & SocketMonitor::READ ? AVAHI_WATCH_IN : 0) |
			       (e & SocketMonitor::WRITE ? AVAHI_WATCH_OUT : 0) |
			       (e & SocketMonitor::ERROR ? AVAHI_WATCH_ERR : 0) |
			       (e & SocketMonitor::HANGUP ? AVAHI_WATCH_HUP : 0));
}

struct AvahiWatch final : private SocketMonitor {
private:
	const AvahiWatchCallback callback;
	void *const userdata;

	AvahiWatchEvent received;

public:
	AvahiWatch(SocketDescriptor _fd, AvahiWatchEvent _event,
		   AvahiWatchCallback _callback, void *_userdata,
		   EventLoop &_loop)
		:SocketMonitor(_fd, _loop),
		 callback(_callback), userdata(_userdata),
		 received(AvahiWatchEvent(0)) {
		Schedule(FromAvahiWatchEvent(_event));
	}

	static void WatchUpdate(AvahiWatch *w, AvahiWatchEvent event) {
		w->Schedule(FromAvahiWatchEvent(event));
	}

	static AvahiWatchEvent WatchGetEvents(AvahiWatch *w) {
		return w->received;
	}

	static void WatchFree(AvahiWatch *w) {
		delete w;
	}

private:
	/* virtual methods from class SocketMonitor */
	bool OnSocketReady(unsigned flags) noexcept {
		received = ToAvahiWatchEvent(flags);
		callback(this, GetSocket().Get(), received, userdata);
		received = AvahiWatchEvent(0);
		return true;
	}
};

struct AvahiTimeout final {
	TimerEvent timer;

	const AvahiTimeoutCallback callback;
	void *const userdata;

public:
	AvahiTimeout(const struct timeval *tv,
		     AvahiTimeoutCallback _callback, void *_userdata,
		     EventLoop &_loop)
		:timer(_loop, BIND_THIS_METHOD(OnTimeout)),
		 callback(_callback), userdata(_userdata) {
		if (tv != nullptr)
			timer.Schedule(ToSteadyClockDuration(*tv));
	}

	static void TimeoutUpdate(AvahiTimeout *t, const struct timeval *tv) {
		if (tv != nullptr)
			t->timer.Schedule(ToSteadyClockDuration(*tv));
		else
			t->timer.Cancel();
	}

	static void TimeoutFree(AvahiTimeout *t) {
		delete t;
	}

private:
	void OnTimeout() noexcept {
		callback(this, userdata);
	}
};

MyAvahiPoll::MyAvahiPoll(EventLoop &_loop):event_loop(_loop)
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
MyAvahiPoll::WatchNew(const AvahiPoll *api, int fd, AvahiWatchEvent event,
		      AvahiWatchCallback callback, void *userdata) {
	const MyAvahiPoll &poll = *(const MyAvahiPoll *)api;

	return new AvahiWatch(SocketDescriptor(fd), event, callback, userdata,
			      poll.event_loop);
}

AvahiTimeout *
MyAvahiPoll::TimeoutNew(const AvahiPoll *api, const struct timeval *tv,
			AvahiTimeoutCallback callback, void *userdata) {
	const MyAvahiPoll &poll = *(const MyAvahiPoll *)api;

	return new AvahiTimeout(tv, callback, userdata,
				poll.event_loop);
}
