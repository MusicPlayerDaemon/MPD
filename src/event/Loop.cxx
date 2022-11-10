/*
 * Copyright 2003-2022 The Music Player Daemon Project
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
#include "DeferEvent.hxx"
#include "SocketEvent.hxx"
#include "util/ScopeExit.hxx"

#ifdef HAVE_THREADED_EVENT_LOOP
#include "InjectEvent.hxx"
#endif

#ifdef HAVE_URING
#include "UringManager.hxx"
#include "util/PrintException.hxx"
#include <stdio.h>
#endif

EventLoop::EventLoop(
#ifdef HAVE_THREADED_EVENT_LOOP
		     ThreadId _thread
#endif
		     )
#ifdef HAVE_THREADED_EVENT_LOOP
	:thread(_thread),
	 /* if this instance is hosted by an EventThread (no ThreadId
	    known yet) then we're not yet alive until the thread is
	    started; for the main EventLoop instance, we assume it's
	    already alive, because nobody but EventThread will call
	    SetAlive() */
	 alive(!_thread.IsNull())
#endif
{
}

EventLoop::~EventLoop() noexcept
{
#if defined(HAVE_URING) && !defined(NDEBUG)
	/* if Run() was never called (maybe because startup failed and
	   an exception is pending), we need to destruct the
	   Uring::Manager here or else the assertions below fail */
	uring.reset();
#endif

	assert(defer.empty());
	assert(idle.empty());
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(inject.empty());
#endif
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
EventLoop::AddFD(int fd, unsigned events, SocketEvent &event) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif
	assert(events != 0);

	if (!poll_backend.Add(fd, events, &event))
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

	return poll_backend.Modify(fd, events, &event);
}

bool
EventLoop::RemoveFD(int fd, SocketEvent &event) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif

	event.unlink();
	return poll_backend.Remove(fd);
}

bool
EventLoop::AbandonFD(SocketEvent &event)  noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif
	assert(event.IsDefined());

	event.unlink();

	return poll_backend.Abandon(event.GetSocket().Get());
}

void
EventLoop::Insert(CoarseTimerEvent &t) noexcept
{
	assert(IsInside());

	coarse_timers.Insert(t, SteadyNow());
	again = true;
}

void
EventLoop::Insert(FineTimerEvent &t) noexcept
{
	assert(IsInside());

	timers.Insert(t);
	again = true;
}

inline Event::Duration
EventLoop::HandleTimers() noexcept
{
	const auto now = SteadyNow();

	auto fine_timeout = timers.Run(now);
	auto coarse_timeout = coarse_timers.Run(now);

	return fine_timeout.count() < 0 ||
		(coarse_timeout.count() >= 0 && coarse_timeout < fine_timeout)
		? coarse_timeout
		: fine_timeout;
}

void
EventLoop::AddDefer(DeferEvent &e) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif

	defer.push_back(e);
	again = true;
}

void
EventLoop::AddIdle(DeferEvent &e) noexcept
{
	assert(IsInside());

	idle.push_back(e);
	again = true;
}

void
EventLoop::RunDeferred() noexcept
{
	while (!defer.empty() && !quit) {
		defer.pop_front_and_dispose([](DeferEvent *e){
			e->Run();
		});
	}
}

bool
EventLoop::RunOneIdle() noexcept
{
	if (idle.empty())
		return false;

	idle.pop_front_and_dispose([](DeferEvent *e){
		e->Run();
	});

	return true;
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
ExportTimeoutMS(Event::Duration timeout) noexcept
{
	return timeout >= timeout.zero()
		? int(duration_cast_round_up<std::chrono::milliseconds>(timeout).count())
		: -1;
}

inline bool
EventLoop::Wait(Event::Duration timeout) noexcept
{
	const auto poll_result =
		poll_backend.ReadEvents(ExportTimeoutMS(timeout));

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

	FlushClockCaches();

	do {
		again = false;

		/* invoke timers */

		const auto timeout = HandleTimers();
		if (quit)
			break;

		RunDeferred();
		if (quit)
			break;

		if (RunOneIdle())
			/* check for other new events after each
			   "idle" invocation to ensure that the other
			   "idle" events are really invoked at the
			   very end */
			continue;

#ifdef HAVE_THREADED_EVENT_LOOP
		/* try to handle DeferEvents without WakeFD
		   overhead */
		{
			const std::scoped_lock<Mutex> lock(mutex);
			HandleInject();
#endif

			if (again)
				/* re-evaluate timers because one of
				   the DeferEvents may have added a
				   new timeout */
				continue;

#ifdef HAVE_THREADED_EVENT_LOOP
			busy = false;
		}
#endif

		/* wait for new event */

		Wait(timeout);

		FlushClockCaches();

#ifdef HAVE_THREADED_EVENT_LOOP
		{
			const std::scoped_lock<Mutex> lock(mutex);
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
EventLoop::AddInject(InjectEvent &d) noexcept
{
	bool must_wake;

	{
		const std::scoped_lock<Mutex> lock(mutex);
		if (d.IsPending())
			return;

		/* we don't need to wake up the EventLoop if another
		   InjectEvent has already done it */
		must_wake = !busy && inject.empty();

		inject.push_back(d);
		again = true;
	}

	if (must_wake)
		wake_fd.Write();
}

void
EventLoop::RemoveInject(InjectEvent &d) noexcept
{
	const std::scoped_lock<Mutex> protect(mutex);

	if (d.IsPending())
		inject.erase(inject.iterator_to(d));
}

void
EventLoop::HandleInject() noexcept
{
	while (!inject.empty() && !quit) {
		auto &m = inject.front();
		assert(m.IsPending());

		inject.pop_front();

		const ScopeUnlock unlock(mutex);
		m.Run();
	}
}

void
EventLoop::OnSocketReady([[maybe_unused]] unsigned flags) noexcept
{
	assert(IsInside());

	wake_fd.Read();

	const std::scoped_lock<Mutex> lock(mutex);
	HandleInject();
}

#endif
