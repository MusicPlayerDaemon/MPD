// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Chrono.hxx"
#include "TimerWheel.hxx"
#include "Backend.hxx"
#include "event/Features.h"
#include "time/ClockCache.hxx"
#include "util/IntrusiveList.hxx"

#ifndef NO_FINE_TIMER_EVENT
#include "TimerList.hxx"
#endif // NO_FINE_TIMER_EVENT

#ifdef HAVE_THREADED_EVENT_LOOP
#include "WakeFD.hxx"
#include "SocketEvent.hxx"
#include "thread/Id.hxx"
#include "thread/Mutex.hxx"
#endif

#include <cassert>

#include "io/uring/Features.h"
#ifdef HAVE_URING
#include <memory>
namespace Uring { class Queue; class Manager; }
#endif

class DeferEvent;
class SocketEvent;
class InjectEvent;

/**
 * An event loop that polls for events on file/socket descriptors.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs it, except where explicitly documented as
 * thread-safe.
 *
 * @see SocketEvent, MultiSocketMonitor, TimerEvent, DeferEvent, InjectEvent
 */
class EventLoop final
{
	EventPollBackend poll_backend;

#ifdef HAVE_THREADED_EVENT_LOOP
	WakeFD wake_fd;
	SocketEvent wake_event{*this, BIND_THIS_METHOD(OnSocketReady), wake_fd.GetSocket()};
#endif

	TimerWheel coarse_timers;

#ifndef NO_FINE_TIMER_EVENT
	TimerList timers;
#endif // NO_FINE_TIMER_EVENT

	using DeferList = IntrusiveList<DeferEvent>;

	DeferList defer;

	/**
	 * This is like #defer, but gets invoked when the loop is idle.
	 */
	DeferList idle;

#ifdef HAVE_THREADED_EVENT_LOOP
	Mutex mutex;

	using InjectList = IntrusiveList<InjectEvent>;
	InjectList inject;
#endif

	using SocketList = IntrusiveList<SocketEvent>;

	/**
	 * A list of scheduled #SocketEvent instances, without those
	 * which are ready (these are in #ready_sockets).
	 */
	SocketList sockets;

	/**
	 * A list of #SocketEvent instances which have a non-zero
	 * "ready_flags" field, and need to be dispatched.
	 */
	SocketList ready_sockets;

#ifdef HAVE_URING
	std::unique_ptr<Uring::Manager> uring;
#endif

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread = ThreadId::Null();

	/**
	 * Is this #EventLoop alive, i.e. can events be scheduled?
	 * This is used by BlockingCall() to determine whether
	 * schedule in the #EventThread or to call directly (if
	 * there's no #EventThread yet/anymore).
	 */
	bool alive;
#endif

	bool quit = false;

	/**
	 * True when the object has been modified and another check is
	 * necessary before going to sleep via EventPollBackend::ReadEvents().
	 */
	bool again;

#ifdef HAVE_THREADED_EVENT_LOOP
	bool quit_injected = false;

	/**
	 * True when handling callbacks, false when waiting for I/O or
	 * timeout.
	 *
	 * Protected with #mutex.
	 */
	bool busy = true;
#endif

#ifdef HAVE_URING
	bool uring_initialized = false;
#endif

	ClockCache<std::chrono::steady_clock> steady_clock_cache;

public:
	/**
	 * Throws on error.
	 */
#ifdef HAVE_THREADED_EVENT_LOOP
	explicit EventLoop(ThreadId _thread);

	EventLoop():EventLoop(ThreadId::GetCurrent()) {}
#else
	EventLoop();
#endif

	~EventLoop() noexcept;

	EventLoop(const EventLoop &other) = delete;
	EventLoop &operator=(const EventLoop &other) = delete;

	const auto &GetSteadyClockCache() const noexcept {
		return steady_clock_cache;
	}

	/**
	 * Caching wrapper for std::chrono::steady_clock::now().  The
	 * real clock is queried at most once per event loop
	 * iteration, because it is assumed that the event loop runs
	 * for a negligible duration.
	 */
	[[gnu::pure]]
	const auto &SteadyNow() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		assert(IsInside());
#endif

		return steady_clock_cache.now();
	}

	void FlushClockCaches() noexcept {
		steady_clock_cache.flush();
	}

#ifdef HAVE_URING
	[[gnu::pure]]
	Uring::Queue *GetUring() noexcept;
#endif

	/**
	 * Stop execution of this #EventLoop at the next chance.
	 *
	 * This method is not thread-safe.  For stopping the
	 * #EventLoop from within another thread, use InjectBreak().
	 */
	void Break() noexcept {
		quit = true;
	}

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Like Break(), but thread-safe.  It is also non-blocking:
	 * after returning, it is not guaranteed that the EventLoop
	 * has really stopped.
	 */
	void InjectBreak() noexcept {
		{
			const std::scoped_lock lock{mutex};
			quit_injected = true;
		}

		wake_fd.Write();
	}
#endif // HAVE_THREADED_EVENT_LOOP

	bool AddFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool RemoveFD(int fd, SocketEvent &event) noexcept;

	/**
	 * Remove the given #SocketEvent after the file descriptor
	 * has been closed.  This is like RemoveFD(), but does not
	 * attempt to use #EPOLL_CTL_DEL.
	 */
	bool AbandonFD(SocketEvent &event) noexcept;

	void Insert(CoarseTimerEvent &t) noexcept;

#ifndef NO_FINE_TIMER_EVENT
	void Insert(FineTimerEvent &t) noexcept;
#endif // NO_FINE_TIMER_EVENT

	/**
	 * Schedule a call to DeferEvent::RunDeferred().
	 */
	void AddDefer(DeferEvent &e) noexcept;
	void AddIdle(DeferEvent &e) noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Schedule a call to the InjectEvent.
	 *
	 * This method is thread-safe.
	 */
	void AddInject(InjectEvent &d) noexcept;

	/**
	 * Cancel a pending call to the InjectEvent.
	 * However after returning, the call may still be running.
	 *
	 * This method is thread-safe.
	 */
	void RemoveInject(InjectEvent &d) noexcept;
#endif

	/**
	 * The main function of this class.  It will loop until
	 * Break() gets called.  Can be called only once.
	 */
	void Run() noexcept;

private:
	void RunDeferred() noexcept;

	/**
	 * Invoke one "idle" #DeferEvent.
	 *
	 * @return false if there was no such event
	 */
	bool RunOneIdle() noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Invoke all pending InjectEvents.
	 *
	 * Caller must lock the mutex.
	 */
	void HandleInject() noexcept;
#endif

	/**
	 * Invoke all expired #TimerEvent instances and return the
	 * duration until the next timer expires.  Returns a negative
	 * duration if there is no timeout.
	 */
	Event::Duration HandleTimers() noexcept;

	/**
	 * Call epoll_wait() and pass all returned events to
	 * SocketEvent::SetReadyFlags().
	 *
	 * @return true if one or more sockets have become ready
	 */
	bool Wait(Event::Duration timeout) noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	void OnSocketReady(unsigned flags) noexcept;
#endif

public:
#ifdef HAVE_THREADED_EVENT_LOOP
	void SetAlive(bool _alive) noexcept {
		alive = _alive;
	}

	bool IsAlive() const noexcept {
		return alive;
	}
#endif

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	[[gnu::pure]]
	bool IsInside() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		return thread.IsInside();
#else
		return true;
#endif
	}
};
