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
#include "TimerEvent.hxx"
#include "SocketEvent.hxx"
#include "IdleEvent.hxx"
#include "util/ScopeExit.hxx"

#ifdef HAVE_THREADED_EVENT_LOOP
#include "DeferEvent.hxx"
#endif

#ifdef HAVE_URING
#include "UringManager.hxx"
#include "util/PrintException.hxx"
#include <stdio.h>
#endif

constexpr bool
EventLoop::TimerCompare::operator()(const TimerEvent &a,
				    const TimerEvent &b) const noexcept
{
	return a.due < b.due;
}

EventLoop::EventLoop(
#ifdef HAVE_THREADED_EVENT_LOOP
		     ThreadId _thread
#endif
		     )
	:
#ifdef HAVE_THREADED_EVENT_LOOP
	wake_event(*this, BIND_THIS_METHOD(OnSocketReady)),
	 thread(_thread),
	 /* if this instance is hosted by an EventThread (no ThreadId
	    known yet) then we're not yet alive until the thread is
	    started; for the main EventLoop instance, we assume it's
	    already alive, because nobody but EventThread will call
	    SetAlive() */
	 alive(!_thread.IsNull()),
#endif
	 quit(false)
{
#ifdef HAVE_THREADED_EVENT_LOOP
	wake_event.Open(SocketDescriptor(wake_fd.Get()));
#endif
}

EventLoop::~EventLoop() noexcept
{
	assert(idle.empty());
	assert(timers.empty());
	assert(sockets.empty());
	assert(ready_sockets.empty());
}

#ifdef HAVE_URING

Uring::Queue *
EventLoop::GetUring() noexcept
{
	if (!uring_initialized) {
		uring_initialized = true;
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

#ifdef HAVE_THREADED_EVENT_LOOP
	wake_fd.Write();
#endif
}

bool
EventLoop::AbandonFD(int _fd)  noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif

	return poll_group.Abandon(_fd);
}

bool
EventLoop::AddFD(int fd, unsigned events, SocketEvent &event) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif
	assert(events != 0);

	if (!poll_group.Add(fd, events, &event))
		return false;

	sockets.push_back(event);
	return true;
}

bool
EventLoop::ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif
	assert(events != 0);

	return poll_group.Modify(fd, events, &event);
}

bool
EventLoop::RemoveFD(int fd, SocketEvent &event) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif

	event.unlink();
	return poll_group.Remove(fd);
}

void
EventLoop::AddIdle(IdleEvent &i) noexcept
{
	assert(IsInside());

	idle.push_back(i);
	again = true;
}

void
EventLoop::AddTimer(TimerEvent &t, Event::Duration d) noexcept
{
	assert(IsInside());

	t.due = now + d;
	timers.insert(t);
	again = true;
}

inline Event::Duration
EventLoop::HandleTimers() noexcept
{
	Event::Duration timeout;

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

	return Event::Duration(-1);
}

template<class ToDuration, class Rep, class Period>
static constexpr ToDuration
duration_cast_round_up(std::chrono::duration<Rep, Period> d) noexcept
{
	using FromDuration = decltype(d);
	constexpr auto one = std::chrono::duration_cast<FromDuration>(ToDuration(1));
	constexpr auto round_add = one > one.zero()
		? one - FromDuration(1)
		: one.zero();
	return std::chrono::duration_cast<ToDuration>(d + round_add);
}

/**
 * Convert the given timeout specification to a milliseconds integer,
 * to be used by functions like poll() and epoll_wait().  Any negative
 * value (= never times out) is translated to the magic value -1.
 */
static constexpr int
ExportTimeoutMS(Event::Duration timeout)
{
	return timeout >= timeout.zero()
		? int(duration_cast_round_up<std::chrono::milliseconds>(timeout).count())
		: -1;
}

inline bool
EventLoop::Wait(Event::Duration timeout) noexcept
{
	const auto poll_result =
		poll_group.ReadEvents(ExportTimeoutMS(timeout));

	for (size_t i = 0; i < poll_result.GetSize(); ++i) {
		auto &socket_event = *(SocketEvent *)poll_result.GetObject(i);
		socket_event.SetReadyFlags(poll_result.GetEvents(i));

		/* move from "sockets" to "ready_sockets" */
		socket_event.unlink();
		ready_sockets.push_back(socket_event);
	}

	return poll_result.GetSize() > 0;
}

void
EventLoop::Run() noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	if (thread.IsNull())
		thread = ThreadId::GetCurrent();
#endif

	assert(IsInside());
	assert(!quit);
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(alive);
	assert(busy);

	wake_event.Schedule(SocketEvent::READ);
#endif

#ifdef HAVE_URING
	AtScopeExit(this) {
		/* make sure that the Uring::Manager gets destructed
		   from within the EventThread, or else its
		   destruction in another thread will cause assertion
		   failures */
		uring.reset();
		uring_initialized = false;
	};
#endif

#ifdef HAVE_THREADED_EVENT_LOOP
	AtScopeExit(this) {
		wake_event.Cancel();
	};
#endif

	do {
		now = std::chrono::steady_clock::now();
		again = false;

		/* invoke timers */

		const auto timeout = HandleTimers();
		if (quit)
			break;

		/* invoke idle */

		while (!idle.empty()) {
			IdleEvent &m = idle.front();
			idle.pop_front();
			m.Run();

			if (quit)
				return;
		}

#ifdef HAVE_THREADED_EVENT_LOOP
		/* try to handle DeferEvents without WakeFD
		   overhead */
		{
			const std::lock_guard<Mutex> lock(mutex);
			HandleDeferred();
			busy = false;

			if (again)
				/* re-evaluate timers because one of
				   the IdleEvents may have added a
				   new timeout */
				continue;
		}
#endif

		/* wait for new event */

		Wait(timeout);

		now = std::chrono::steady_clock::now();

#ifdef HAVE_THREADED_EVENT_LOOP
		{
			const std::lock_guard<Mutex> lock(mutex);
			busy = true;
		}
#endif

		/* invoke sockets */
		while (!ready_sockets.empty() && !quit) {
			auto &socket_event = ready_sockets.front();

			/* move from "ready_sockets" back to "sockets" */
			socket_event.unlink();
			sockets.push_back(socket_event);

			socket_event.Dispatch();
		}
	} while (!quit);

#ifdef HAVE_THREADED_EVENT_LOOP
#ifndef NDEBUG
	assert(thread.IsInside());
#endif
#endif
}

#ifdef HAVE_THREADED_EVENT_LOOP

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

void
EventLoop::OnSocketReady([[maybe_unused]] unsigned flags) noexcept
{
	assert(IsInside());

	wake_fd.Read();

	const std::lock_guard<Mutex> lock(mutex);
	HandleDeferred();
}

#endif
