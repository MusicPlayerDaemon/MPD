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

#ifndef MPD_EVENT_LOOP_HXX
#define MPD_EVENT_LOOP_HXX

#include "Chrono.hxx"
#include "Backend.hxx"
#include "SocketEvent.hxx"
#include "event/Features.h"
#include "util/Compiler.h"
#include "util/IntrusiveList.hxx"

#ifdef HAVE_THREADED_EVENT_LOOP
#include "WakeFD.hxx"
#include "thread/Id.hxx"
#include "thread/Mutex.hxx"
#endif

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>

#include <atomic>
#include <cassert>
#include <chrono>

#include "io/uring/Features.h"
#ifdef HAVE_URING
#include <memory>
namespace Uring { class Queue; class Manager; }
#endif

class TimerEvent;
class IdleEvent;
class DeferEvent;

/**
 * An event loop that polls for events on file/socket descriptors.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs it, except where explicitly documented as
 * thread-safe.
 *
 * @see SocketEvent, MultiSocketMonitor, TimerEvent, IdleEvent
 */
class EventLoop final
{
#ifdef HAVE_THREADED_EVENT_LOOP
	WakeFD wake_fd;
	SocketEvent wake_event;
#endif

	struct TimerCompare {
		constexpr bool operator()(const TimerEvent &a,
					  const TimerEvent &b) const noexcept;
	};

	using TimerSet =
		boost::intrusive::multiset<TimerEvent,
					   boost::intrusive::base_hook<boost::intrusive::set_base_hook<boost::intrusive::link_mode<boost::intrusive::auto_unlink>>>,
					   boost::intrusive::compare<TimerCompare>,
					   boost::intrusive::constant_time_size<false>>;
	TimerSet timers;

	using IdleList = IntrusiveList<IdleEvent>;
	IdleList idle;

#ifdef HAVE_THREADED_EVENT_LOOP
	Mutex mutex;

	using DeferredList =
		boost::intrusive::list<DeferEvent,
				       boost::intrusive::base_hook<boost::intrusive::list_base_hook<>>,
				       boost::intrusive::constant_time_size<false>>;
	DeferredList deferred;
#endif

	using SocketList = IntrusiveList<SocketEvent>;

	/**
	 * A list of scheduled #SocketEvent instances, without those
	 * which are ready (these are in #ready_sockets).
	 */
	SocketList sockets;

	/**
	 * A linked list of #SocketEvent instances which have a
	 * non-zero "ready_flags" field, and need to be dispatched.
	 */
	SocketList ready_sockets;

#ifdef HAVE_URING
	std::unique_ptr<Uring::Manager> uring;
#endif

	Event::Clock::time_point now = Event::Clock::now();

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

	std::atomic_bool quit;

	/**
	 * True when the object has been modified and another check is
	 * necessary before going to sleep via EventPollBackend::ReadEvents().
	 */
	bool again;

#ifdef HAVE_THREADED_EVENT_LOOP
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

	EventPollBackend poll_backend;

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

	/**
	 * A caching wrapper for Event::Clock::now().
	 */
	auto GetTime() const {
#ifdef HAVE_THREADED_EVENT_LOOP
		assert(IsInside());
#endif

		return now;
	}

#ifdef HAVE_URING
	gcc_pure
	Uring::Queue *GetUring() noexcept;
#endif

	/**
	 * Stop execution of this #EventLoop at the next chance.  This
	 * method is thread-safe and non-blocking: after returning, it
	 * is not guaranteed that the EventLoop has really stopped.
	 */
	void Break() noexcept;

	bool AddFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool ModifyFD(int fd, unsigned events, SocketEvent &event) noexcept;
	bool RemoveFD(int fd, SocketEvent &event) noexcept;

	/**
	 * Remove the given #SocketEvent after the file descriptor
	 * has been closed.  This is like RemoveFD(), but does not
	 * attempt to use #EPOLL_CTL_DEL.
	 */
	bool AbandonFD(int fd) noexcept;

	void AddIdle(IdleEvent &i) noexcept;

	void AddTimer(TimerEvent &t, Event::Duration d) noexcept;

#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Schedule a call to DeferEvent::RunDeferred().
	 *
	 * This method is thread-safe.
	 */
	void AddDeferred(DeferEvent &d) noexcept;

	/**
	 * Cancel a pending call to DeferEvent::RunDeferred().
	 * However after returning, the call may still be running.
	 *
	 * This method is thread-safe.
	 */
	void RemoveDeferred(DeferEvent &d) noexcept;
#endif

	/**
	 * The main function of this class.  It will loop until
	 * Break() gets called.  Can be called only once.
	 */
	void Run() noexcept;

private:
#ifdef HAVE_THREADED_EVENT_LOOP
	/**
	 * Invoke all pending DeferEvents.
	 *
	 * Caller must lock the mutex.
	 */
	void HandleDeferred() noexcept;
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
	gcc_pure
	bool IsInside() const noexcept {
#ifdef HAVE_THREADED_EVENT_LOOP
		return thread.IsInside();
#else
		return true;
#endif
	}
};

#endif /* MAIN_NOTIFY_H */
