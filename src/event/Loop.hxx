/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_EVENT_LOOP_HXX
#define MPD_EVENT_LOOP_HXX

#include "check.h"
#include "thread/Id.hxx"
#include "gcc.h"

#ifdef USE_EPOLL
#include "system/EPollFD.hxx"
#include "thread/Mutex.hxx"
#include "WakeFD.hxx"
#include "SocketMonitor.hxx"

#include <functional>
#include <list>
#include <set>
#else
#include <glib.h>
#endif

#ifdef USE_EPOLL
class TimeoutMonitor;
class IdleMonitor;
class SocketMonitor;
#endif

#include <assert.h>

class EventLoop final
#ifdef USE_EPOLL
	: private SocketMonitor
#endif
{
#ifdef USE_EPOLL
	struct TimerRecord {
		/**
		 * Projected monotonic_clock_ms() value when this
		 * timer is due.
		 */
		const unsigned due_ms;

		TimeoutMonitor &timer;

		constexpr TimerRecord(TimeoutMonitor &_timer,
				      unsigned _due_ms)
			:due_ms(_due_ms), timer(_timer) {}

		bool operator<(const TimerRecord &other) const {
			return due_ms < other.due_ms;
		}

		bool IsDue(unsigned _now_ms) const {
			return _now_ms >= due_ms;
		}
	};

	EPollFD epoll;

	WakeFD wake_fd;

	std::multiset<TimerRecord> timers;
	std::list<IdleMonitor *> idle;

	Mutex mutex;
	std::list<std::function<void()>> calls;

	unsigned now_ms;

	bool quit;

	static constexpr unsigned MAX_EVENTS = 16;
	unsigned n_events;
	epoll_event events[MAX_EVENTS];
#else
	GMainContext *context;
	GMainLoop *loop;
#endif

	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread;

public:
#ifdef USE_EPOLL
	struct Default {};

	EventLoop(Default dummy=Default());
	~EventLoop();

	unsigned GetTimeMS() const {
		return now_ms;
	}

	void Break();

	bool AddFD(int _fd, unsigned flags, SocketMonitor &m) {
		return epoll.Add(_fd, flags, &m);
	}

	bool ModifyFD(int _fd, unsigned flags, SocketMonitor &m) {
		return epoll.Modify(_fd, flags, &m);
	}

	bool RemoveFD(int fd, SocketMonitor &m);

	void AddIdle(IdleMonitor &i);
	void RemoveIdle(IdleMonitor &i);

	void AddTimer(TimeoutMonitor &t, unsigned ms);
	void CancelTimer(TimeoutMonitor &t);

	void AddCall(std::function<void()> &&f);

	void Run();

private:
	virtual bool OnSocketReady(unsigned flags) override;

public:
#else
	EventLoop()
		:context(g_main_context_new()),
		 loop(g_main_loop_new(context, false)),
		 thread(ThreadId::Null()) {}

	struct Default {};
	EventLoop(gcc_unused Default _dummy)
		:context(g_main_context_ref(g_main_context_default())),
		 loop(g_main_loop_new(context, false)),
		 thread(ThreadId::Null()) {}

	~EventLoop() {
		g_main_loop_unref(loop);
		g_main_context_unref(context);
	}

	GMainContext *GetContext() {
		return context;
	}

	void WakeUp() {
		g_main_context_wakeup(context);
	}

	void Break() {
		g_main_loop_quit(loop);
	}

	void Run();

	guint AddIdle(GSourceFunc function, gpointer data);

	GSource *AddTimeout(guint interval_ms,
			    GSourceFunc function, gpointer data);

	GSource *AddTimeoutSeconds(guint interval_s,
				   GSourceFunc function, gpointer data);
#endif

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	gcc_pure
	bool IsInside() const {
		assert(!thread.IsNull());

		return thread.IsInside();
	}
};

#endif /* MAIN_NOTIFY_H */
