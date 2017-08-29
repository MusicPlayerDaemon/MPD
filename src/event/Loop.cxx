/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "SocketMonitor.hxx"
#include "IdleMonitor.hxx"
#include "DeferredMonitor.hxx"
#include "util/ScopeExit.hxx"

#include <algorithm>

EventLoop::EventLoop(ThreadId _thread)
	:SocketMonitor(*this), thread(_thread)
{
	SocketMonitor::Open(SocketDescriptor(wake_fd.Get()));
}

EventLoop::~EventLoop()
{
	assert(idle.empty());
	assert(timers.empty());
}

void
EventLoop::Break()
{
	if (quit)
		return;

	quit = true;
	wake_fd.Write();
}

bool
EventLoop::Abandon(int _fd, SocketMonitor &m)
{
	assert(IsInside());

	poll_result.Clear(&m);
	return poll_group.Abandon(_fd);
}

bool
EventLoop::RemoveFD(int _fd, SocketMonitor &m)
{
	assert(IsInside());

	poll_result.Clear(&m);
	return poll_group.Remove(_fd);
}

void
EventLoop::AddIdle(IdleMonitor &i)
{
	assert(IsInside());
	assert(std::find(idle.begin(), idle.end(), &i) == idle.end());

	idle.push_back(&i);
	again = true;
}

void
EventLoop::RemoveIdle(IdleMonitor &i)
{
	assert(IsInside());

	auto it = std::find(idle.begin(), idle.end(), &i);
	assert(it != idle.end());

	idle.erase(it);
}

void
EventLoop::AddTimer(TimeoutMonitor &t, std::chrono::steady_clock::duration d)
{
	assert(IsInside());

	t.due = now + d;
	timers.insert(t);
	again = true;
}

void
EventLoop::CancelTimer(TimeoutMonitor &t)
{
	assert(IsInside());

	timers.erase(timers.iterator_to(t));
}

/**
 * Convert the given timeout specification to a milliseconds integer,
 * to be used by functions like poll() and epoll_wait().  Any negative
 * value (= never times out) is translated to the magic value -1.
 */
static constexpr int
ExportTimeoutMS(std::chrono::steady_clock::duration timeout)
{
	return timeout >= timeout.zero()
		? int(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count())
		: -1;
}

void
EventLoop::Run()
{
	if (thread.IsNull())
		thread = ThreadId::GetCurrent();

	assert(IsInside());
	assert(!quit);
	assert(busy);

	SocketMonitor::Schedule(SocketMonitor::READ);
	AtScopeExit(this) { SocketMonitor::Cancel(); };

	do {
		now = std::chrono::steady_clock::now();
		again = false;

		/* invoke timers */

		std::chrono::steady_clock::duration timeout;
		while (true) {
			auto i = timers.begin();
			if (i == timers.end()) {
				timeout = std::chrono::steady_clock::duration(-1);
				break;
			}

			TimeoutMonitor &m = *i;
			timeout = m.due - now;
			if (timeout > timeout.zero())
				break;

			timers.erase(i);

			m.Run();

			if (quit)
				return;
		}

		/* invoke idle */

		while (!idle.empty()) {
			IdleMonitor &m = *idle.front();
			idle.pop_front();
			m.Run();

			if (quit)
				return;
		}

		/* try to handle DeferredMonitors without WakeFD
		   overhead */
		{
			const std::lock_guard<Mutex> lock(mutex);
			HandleDeferred();
			busy = false;

			if (again)
				/* re-evaluate timers because one of
				   the IdleMonitors may have added a
				   new timeout */
				continue;
		}

		/* wait for new event */

		poll_group.ReadEvents(poll_result, ExportTimeoutMS(timeout));

		now = std::chrono::steady_clock::now();

		{
			const std::lock_guard<Mutex> lock(mutex);
			busy = true;
		}

		/* invoke sockets */
		for (int i = 0; i < poll_result.GetSize(); ++i) {
			auto events = poll_result.GetEvents(i);
			if (events != 0) {
				if (quit)
					break;

				auto m = (SocketMonitor *)poll_result.GetObject(i);
				m->Dispatch(events);
			}
		}

		poll_result.Reset();

	} while (!quit);

#ifndef NDEBUG
	assert(busy);
	assert(thread.IsInside());
#endif
}

void
EventLoop::AddDeferred(DeferredMonitor &d)
{
	bool must_wake;

	{
		const std::lock_guard<Mutex> lock(mutex);
		if (d.pending)
			return;

		assert(std::find(deferred.begin(),
				 deferred.end(), &d) == deferred.end());

		/* we don't need to wake up the EventLoop if another
		   DeferredMonitor has already done it */
		must_wake = !busy && deferred.empty();

		d.pending = true;
		deferred.push_back(&d);
		again = true;
	}

	if (must_wake)
		wake_fd.Write();
}

void
EventLoop::RemoveDeferred(DeferredMonitor &d)
{
	const std::lock_guard<Mutex> protect(mutex);

	if (!d.pending) {
		assert(std::find(deferred.begin(),
				 deferred.end(), &d) == deferred.end());
		return;
	}

	d.pending = false;

	auto i = std::find(deferred.begin(), deferred.end(), &d);
	assert(i != deferred.end());

	deferred.erase(i);
}

void
EventLoop::HandleDeferred()
{
	while (!deferred.empty() && !quit) {
		DeferredMonitor &m = *deferred.front();
		assert(m.pending);

		deferred.pop_front();
		m.pending = false;

		const ScopeUnlock unlock(mutex);
		m.RunDeferred();
	}
}

bool
EventLoop::OnSocketReady(gcc_unused unsigned flags)
{
	assert(IsInside());

	wake_fd.Read();

	const std::lock_guard<Mutex> lock(mutex);
	HandleDeferred();

	return true;
}
