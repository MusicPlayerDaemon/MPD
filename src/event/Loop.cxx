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

#include "config.h"
#include "Loop.hxx"

#ifdef USE_INTERNAL_EVENTLOOP

#include "system/Clock.hxx"
#include "TimeoutMonitor.hxx"
#include "SocketMonitor.hxx"
#include "IdleMonitor.hxx"

#include <algorithm>

EventLoop::EventLoop(Default)
	:SocketMonitor(*this),
	 now_ms(::MonotonicClockMS()),
	 quit(false),
	 thread(ThreadId::Null())
{
	SocketMonitor::Open(wake_fd.Get());
	SocketMonitor::Schedule(SocketMonitor::READ);
}

EventLoop::~EventLoop()
{
	assert(idle.empty());
	assert(timers.empty());

	/* avoid closing the WakeFD twice */
	SocketMonitor::Steal();
}

void
EventLoop::Break()
{
	if (IsInside())
		quit = true;
	else
		AddCall([this]() { Break(); });
}

bool
EventLoop::Abandon(int _fd, SocketMonitor &m)
{
	poll_result.Clear(&m);
	return poll_group.Abandon(_fd);
}

bool
EventLoop::RemoveFD(int _fd, SocketMonitor &m)
{
	poll_result.Clear(&m);
	return poll_group.Remove(_fd);
}

void
EventLoop::AddIdle(IdleMonitor &i)
{
	assert(std::find(idle.begin(), idle.end(), &i) == idle.end());

	idle.push_back(&i);
}

void
EventLoop::RemoveIdle(IdleMonitor &i)
{
	auto it = std::find(idle.begin(), idle.end(), &i);
	assert(it != idle.end());

	idle.erase(it);
}

void
EventLoop::AddTimer(TimeoutMonitor &t, unsigned ms)
{
	timers.insert(TimerRecord(t, now_ms + ms));
}

void
EventLoop::CancelTimer(TimeoutMonitor &t)
{
	for (auto i = timers.begin(), end = timers.end(); i != end; ++i) {
		if (&i->timer == &t) {
			timers.erase(i);
			return;
		}
	}
}

#endif

void
EventLoop::Run()
{
	assert(thread.IsNull());
	thread = ThreadId::GetCurrent();

#ifdef USE_INTERNAL_EVENTLOOP
	assert(!quit);

	do {
		now_ms = ::MonotonicClockMS();

		/* invoke timers */

		int timeout_ms;
		while (true) {
			auto i = timers.begin();
			if (i == timers.end()) {
				timeout_ms = -1;
				break;
			}

			timeout_ms = i->due_ms - now_ms;
			if (timeout_ms > 0)
				break;

			TimeoutMonitor &m = i->timer;
			timers.erase(i);

			m.Run();

			if (quit)
				return;
		}

		/* invoke idle */

		const bool idle_empty = idle.empty();
		while (!idle.empty()) {
			IdleMonitor &m = *idle.front();
			idle.pop_front();
			m.Run();

			if (quit)
				return;
		}

		if (!idle_empty)
			/* re-evaluate timers because one of the
			   IdleMonitors may have added a new
			   timeout */
			continue;

		/* wait for new event */

		poll_group.ReadEvents(poll_result, timeout_ms);

		now_ms = ::MonotonicClockMS();

		assert(!quit);

		/* invoke sockets */
		for (int i = 0; i < poll_result.GetSize(); ++i) {
			auto events = poll_result.GetEvents(i);
			if (events != 0) {
				auto m = (SocketMonitor *)poll_result.GetObject(i);
				m->Dispatch(events);

				if (quit)
					break;
			}
		}

		poll_result.Reset();

	} while (!quit);
#endif

#ifdef USE_GLIB_EVENTLOOP
	g_main_loop_run(loop);
#endif

	assert(thread.IsInside());
}

#ifdef USE_INTERNAL_EVENTLOOP

void
EventLoop::AddCall(std::function<void()> &&f)
{
	mutex.lock();
	calls.push_back(f);
	mutex.unlock();

	wake_fd.Write();
}

bool
EventLoop::OnSocketReady(gcc_unused unsigned flags)
{
	assert(!quit);

	wake_fd.Read();

	mutex.lock();

	while (!calls.empty() && !quit) {
		auto f = std::move(calls.front());
		calls.pop_front();

		mutex.unlock();
		f();
		mutex.lock();
	}

	mutex.unlock();

	return true;
}

#endif

#ifdef USE_GLIB_EVENTLOOP

guint
EventLoop::AddIdle(GSourceFunc function, gpointer data)
{
	GSource *source = g_idle_source_new();
	g_source_set_callback(source, function, data, nullptr);
	guint id = g_source_attach(source, GetContext());
	g_source_unref(source);
	return id;
}

GSource *
EventLoop::AddTimeout(guint interval_ms,
		      GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new(interval_ms);
	g_source_set_callback(source, function, data, nullptr);
	g_source_attach(source, GetContext());
	return source;
}

GSource *
EventLoop::AddTimeoutSeconds(guint interval_s,
			     GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new_seconds(interval_s);
	g_source_set_callback(source, function, data, nullptr);
	g_source_attach(source, GetContext());
	return source;
}

#endif
