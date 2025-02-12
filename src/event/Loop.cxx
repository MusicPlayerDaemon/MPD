// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Loop.hxx"
#include "DeferEvent.hxx"
#include "SocketEvent.hxx"
#include "util/ScopeExit.hxx"

#ifdef HAVE_THREADED_EVENT_LOOP
#include "InjectEvent.hxx"
#endif

#ifdef HAVE_URING
#include "uring/Manager.hxx"
#include "io/uring/Operation.hxx"
#include "io/uring/Queue.hxx"
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
	uring_poll.reset();
	uring.reset();
#endif

	assert(defer.empty());
	assert(idle.empty());
	assert(next.empty());
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(inject.empty());
#endif
	assert(sockets.empty());
	assert(ready_sockets.empty());
}

void
EventLoop::SetVolatile() noexcept
{
}

#ifdef HAVE_URING

class EventLoop::UringPoll final : Uring::Operation {
	EventLoop &event_loop;

public:
	UringPoll(EventLoop &_event_loop) noexcept
		:event_loop(_event_loop) {}

	void Start();

private:
	void OnUringCompletion(int res) noexcept override {
		(void)res; // TODO

		event_loop.epoll_ready = true;
	}
};

inline void
EventLoop::UringPoll::Start()
{
	assert(!IsUringPending());
	assert(event_loop.GetUring());

	auto &queue = *event_loop.GetUring();

	auto &s = queue.RequireSubmitEntry();
	io_uring_prep_poll_multishot(&s, event_loop.poll_backend.GetFileDescriptor().Get(), EPOLLIN);
	queue.Push(s, *this);
}

void
EventLoop::EnableUring(unsigned entries, unsigned flags)
{
	assert(!uring);

	uring = std::make_unique<Uring::Manager>(entries, flags);
}

void
EventLoop::EnableUring(unsigned entries, struct io_uring_params &params)
{
	assert(!uring);

	uring = std::make_unique<Uring::Manager>(entries, params);
}

void
EventLoop::DisableUring() noexcept
{
	uring_poll.reset();
	uring.reset();
}

Uring::Queue *
EventLoop::GetUring() noexcept
{
	return uring.get();
}

#endif // HAVE_URING

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

#ifndef NO_FINE_TIMER_EVENT

void
EventLoop::Insert(FineTimerEvent &t) noexcept
{
	assert(IsInside());

	timers.Insert(t);
	again = true;
}

#endif // NO_FINE_TIMER_EVENT

/**
 * Determines which timeout will happen earlier; either one may be
 * negative to specify "no timeout at all".
 */
static constexpr Event::Duration
GetEarlierTimeout(Event::Duration a, Event::Duration b) noexcept
{
	return b.count() < 0 || (a.count() >= 0 && a < b)
		? a
		: b;
}

inline Event::Duration
EventLoop::HandleTimers() noexcept
{
	const auto now = SteadyNow();

#ifndef NO_FINE_TIMER_EVENT
	auto fine_timeout = timers.Run(now);
#else
	const Event::Duration fine_timeout{-1};
#endif // NO_FINE_TIMER_EVENT
	auto coarse_timeout = coarse_timers.Run(now);

	return GetEarlierTimeout(coarse_timeout, fine_timeout);
}

void
EventLoop::AddDefer(DeferEvent &e) noexcept
{
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(!IsAlive() || IsInside());
#endif

	defer.push_back(e);

#ifdef HAVE_THREADED_EVENT_LOOP
	/* setting this flag here is only relevant if we've been
	   called by a DeferEvent */
	again = true;
#endif
}

void
EventLoop::AddIdle(DeferEvent &e) noexcept
{
	assert(IsInside());

	idle.push_back(e);

#ifdef HAVE_THREADED_EVENT_LOOP
	/* setting this flag here is only relevant if we've been
	   called by a DeferEvent */
	again = true;
#endif
}

void
EventLoop::AddNext(DeferEvent &e) noexcept
{
	assert(IsInside());

	next.push_back(e);
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

/**
 * Convert the given timeout specification to a milliseconds integer,
 * to be used by functions like poll() and epoll_wait().  Any negative
 * value (= never times out) is translated to the magic value -1.
 */
static constexpr int
ExportTimeoutMS(Event::Duration timeout) noexcept
{
	return timeout >= timeout.zero()
		? static_cast<int>(std::chrono::ceil<std::chrono::milliseconds>(timeout).count())
		: -1;
}

#ifdef HAVE_URING

static struct __kernel_timespec *
ExportTimeoutKernelTimespec(Event::Duration timeout, struct __kernel_timespec &buffer) noexcept
{
	if (timeout < timeout.zero())
		return nullptr;

	if (timeout >= std::chrono::duration_cast<Event::Duration>(std::chrono::hours{24})) [[unlikely]] {
		buffer = {
			.tv_sec = std::chrono::ceil<std::chrono::duration<__kernel_time64_t>>(timeout).count(),
		};
		return &buffer;
	}

	const auto nsec = std::chrono::ceil<std::chrono::nanoseconds>(timeout);
	buffer = {
		.tv_sec = nsec.count() / 1000000000,
		.tv_nsec = nsec.count() % 1000000000,
	};
	return &buffer;
}

#endif

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
	assert(!thread.IsNull());
#endif

	assert(IsInside());
#ifdef HAVE_THREADED_EVENT_LOOP
	assert(alive || quit_injected);
	assert(busy);

	wake_event.Schedule(SocketEvent::READ);
#endif

#ifdef HAVE_THREADED_EVENT_LOOP
	AtScopeExit(this) {
		wake_event.Cancel();
	};
#endif

	FlushClockCaches();

	while (!quit) {
		again = false;

		/* invoke timers */

		Event::Duration timeout = HandleTimers();
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
			const std::scoped_lock lock{mutex};
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

		if (!next.empty())
			timeout = Event::Duration{0};

#ifdef HAVE_URING
		if (uring) {
			/* use io_uring_enter() and invoke
			   epoll_wait() only if it's reported
			   to be ready */

			if (!uring_poll) [[unlikely]] {
				/* start polling on the epoll
				   file descriptor */
				uring_poll = std::make_unique<UringPoll>(*this);
				uring_poll->Start();
			}

			/* repeat epoll_wait() until it
			   returns no more events; this is a
			   temporary workaround because
			   io_uring_prep_poll_multishot() is
			   edge-triggered, so we have to
			   consume all events to rearm it */

			if (!epoll_ready) {
				struct __kernel_timespec timeout_buffer;
				auto *kernel_timeout = ExportTimeoutKernelTimespec(timeout, timeout_buffer);
				Uring::Queue &uring_queue = *uring;
				uring_queue.SubmitAndWaitDispatchCompletions(kernel_timeout);
			}

			if (epoll_ready) {
				/* invoke epoll_wait() */
				epoll_ready = Wait(Event::Duration{0});
			}
		} else
#endif
			Wait(timeout);

		idle.splice(std::next(idle.begin()), next);

		FlushClockCaches();

#ifdef HAVE_THREADED_EVENT_LOOP
		{
			const std::scoped_lock lock{mutex};
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
	}

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
		const std::scoped_lock lock{mutex};
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
	const std::scoped_lock protect{mutex};

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

	if (quit_injected) {
		Break();
		return;
	}

	const std::scoped_lock lock{mutex};
	HandleInject();
}

#endif
