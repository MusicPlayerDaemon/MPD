/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "Loop.hxx"
#include "SocketMonitor.hxx"
#include "IdleMonitor.hxx"
#include "DeferEvent.hxx"
#include "util/ScopeExit.hxx"

#ifdef HAVE_URING
#include "UringManager.hxx"
#include "util/PrintException.hxx"
#include <stdio.h>
#endif

EventLoop::EventLoop(ThreadId _thread)
	:SocketMonitor(*this),
	 /* if this instance is hosted by an EventThread (no ThreadId
	    known yet) then we're not yet alive until the thread is
	    started; for the main EventLoop instance, we assume it's
	    already alive, because nobody but EventThread will call
	    SetAlive() */
	 alive(!_thread.IsNull()),
	 quit(false),
	 thread(_thread)
{
	SocketMonitor::Open(SocketDescriptor(wake_fd.Get()));
}

EventLoop::~EventLoop() noexcept
{
	assert(idle.empty());
	assert(timers.empty());
}

#ifdef HAVE_URING

Uring::Queue *
EventLoop::GetUring() noexcept
{
	if (!uring_initialized) {
		try {
			uring = std::make_unique<Uring::Manager>(*this);
		} catch (...) {
			fprintf(stderr, "Failed to initialize io_uring: ");
			PrintException(std::current_exception());
		}
	}

	return uring.get();
}

#endif

void
EventLoop::Break() noexcept
{
	if (quit.exchange(true))
		return;

	wake_fd.Write();
}

bool
EventLoop::Abandon(int _fd, SocketMonitor &m)  noexcept
{
	assert(!IsAlive() || IsInside());

	poll_result.Clear(&m);
	return poll_group.Abandon(_fd);
}

bool
EventLoop::RemoveFD(int _fd, SocketMonitor &m) noexcept
{
	assert(!IsAlive() || IsInside());

	poll_result.Clear(&m);
	return poll_group.Remove(_fd);
}

void
EventLoop::AddIdle(IdleMonitor &i) noexcept
{
	assert(IsInside());

	idle.push_back(i);
	again = true;
}

void
EventLoop::RemoveIdle(IdleMonitor &i) noexcept
{
	assert(IsInside());

	idle.erase(idle.iterator_to(i));
}

void
EventLoop::AddTimer(TimerEvent &t, std::chrono::steady_clock::duration d) noexcept
{
	assert(IsInside());

	t.due = now + d;
	timers.insert(t);
	again = true;
}

void
EventLoop::CancelTimer(TimerEvent &t) noexcept
{
	assert(IsInside());

	timers.erase(timers.iterator_to(t));
}

inline std::chrono::steady_clock::duration
EventLoop::HandleTimers() noexcept
{
	std::chrono::steady_clock::duration timeout;

	while (!quit) {
		auto i = timers.begin();
		if (i == timers.end())
			break;

		TimerEvent &t = *i;
		timeout = t.due - now;
		if (timeout > timeout.zero())
			return timeout;

		timers.erase(i);

		t.Run();
	}

	return std::chrono::steady_clock::duration(-1);
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
		/* round up (+1) to avoid unnecessary wakeups */
		? int(std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count()) + 1
		: -1;
}

void
EventLoop::Run() noexcept
{
	if (thread.IsNull())
		thread = ThreadId::GetCurrent();

	assert(IsInside());
	assert(!quit);
	assert(alive);
	assert(busy);

	SocketMonitor::Schedule(SocketMonitor::READ);
	AtScopeExit(this) {
#ifdef HAVE_URING
		/* make sure that the Uring::Manager gets destructed
		   from within the EventThread, or else its
		   destruction in another thread will cause assertion
		   failures */
		uring.reset();
		uring_initialized = false;
#endif

		SocketMonitor::Cancel();
	};

	do {
		now = std::chrono::steady_clock::now();
		again = false;

		/* invoke timers */

		const auto timeout = HandleTimers();
		if (quit)
			break;

		/* invoke idle */

		while (!idle.empty()) {
			IdleMonitor &m = idle.front();
			idle.pop_front();
			m.Run();

			if (quit)
				return;
		}

		/* try to handle DeferEvents without WakeFD
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
		for (size_t i = 0; i < poll_result.GetSize(); ++i) {
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
	assert(thread.IsInside());
#endif
}

void
EventLoop::AddDeferred(DeferEvent &d) noexcept
{
	bool must_wake;

	{
		const std::lock_guard<Mutex> lock(mutex);
		if (d.IsPending())
			return;

		/* we don't need to wake up the EventLoop if another
		   DeferEvent has already done it */
		must_wake = !busy && deferred.empty();

		deferred.push_back(d);
		again = true;
	}

	if (must_wake)
		wake_fd.Write();
}

void
EventLoop::RemoveDeferred(DeferEvent &d) noexcept
{
	const std::lock_guard<Mutex> protect(mutex);

	if (d.IsPending())
		deferred.erase(deferred.iterator_to(d));
}

void
EventLoop::HandleDeferred() noexcept
{
	while (!deferred.empty() && !quit) {
		auto &m = deferred.front();
		assert(m.IsPending());

		deferred.pop_front();

		const ScopeUnlock unlock(mutex);
		m.RunDeferred();
	}
}

bool
EventLoop::OnSocketReady([[maybe_unused]] unsigned flags) noexcept
{
	assert(IsInside());

	wake_fd.Read();

	const std::lock_guard<Mutex> lock(mutex);
	HandleDeferred();

	return true;
}
