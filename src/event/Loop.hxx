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

#ifndef MPD_EVENT_LOOP_HXX
#define MPD_EVENT_LOOP_HXX

#include "thread/Id.hxx"
#include "util/Compiler.h"

#include "PollGroup.hxx"
#include "thread/Mutex.hxx"
#include "WakeFD.hxx"
#include "SocketMonitor.hxx"
#include "TimerEvent.hxx"
#include "IdleMonitor.hxx"
#include "DeferEvent.hxx"

#include <boost/intrusive/set.hpp>
#include <boost/intrusive/list.hpp>

#include <chrono>
#include <atomic>

#include <assert.h>

/**
 * An event loop that polls for events on file/socket descriptors.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs it, except where explicitly documented as
 * thread-safe.
 *
 * @see SocketMonitor, MultiSocketMonitor, TimerEvent, IdleMonitor
 */
class EventLoop final : SocketMonitor
{
	WakeFD wake_fd;

	struct TimerCompare {
		constexpr bool operator()(const TimerEvent &a,
					  const TimerEvent &b) const {
			return a.due < b.due;
		}
	};

	typedef boost::intrusive::multiset<TimerEvent,
					   boost::intrusive::member_hook<TimerEvent,
									 TimerEvent::TimerSetHook,
									 &TimerEvent::timer_set_hook>,
					   boost::intrusive::compare<TimerCompare>,
					   boost::intrusive::constant_time_size<false>> TimerSet;
	TimerSet timers;

	typedef boost::intrusive::list<IdleMonitor,
				       boost::intrusive::member_hook<IdleMonitor,
								     IdleMonitor::ListHook,
								     &IdleMonitor::list_hook>,
				       boost::intrusive::constant_time_size<false>> IdleList;
	IdleList idle;

	Mutex mutex;

	typedef boost::intrusive::list<DeferEvent,
				       boost::intrusive::member_hook<DeferEvent,
								     DeferEvent::ListHook,
								     &DeferEvent::list_hook>,
				       boost::intrusive::constant_time_size<false>> DeferredList;
	DeferredList deferred;

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	/**
	 * Is this #EventLoop alive, i.e. can events be scheduled?
	 * This is used by BlockingCall() to determine whether
	 * schedule in the #EventThread or to call directly (if
	 * there's no #EventThread yet/anymore).
	 */
	bool alive;

	std::atomic_bool quit;

	/**
	 * True when the object has been modified and another check is
	 * necessary before going to sleep via PollGroup::ReadEvents().
	 */
	bool again;

	/**
	 * True when handling callbacks, false when waiting for I/O or
	 * timeout.
	 *
	 * Protected with #mutex.
	 */
	bool busy = true;

	PollGroup poll_group;
	PollResult poll_result;

	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread = ThreadId::Null();

public:
	/**
	 * Throws on error.
	 */
	explicit EventLoop(ThreadId _thread);

	EventLoop():EventLoop(ThreadId::GetCurrent()) {}

	~EventLoop() noexcept;

	/**
	 * A caching wrapper for std::chrono::steady_clock::now().
	 */
	std::chrono::steady_clock::time_point GetTime() const {
		assert(IsInside());

		return now;
	}

	/**
	 * Stop execution of this #EventLoop at the next chance.  This
	 * method is thread-safe and non-blocking: after returning, it
	 * is not guaranteed that the EventLoop has really stopped.
	 */
	void Break() noexcept;

	bool AddFD(int _fd, unsigned flags, SocketMonitor &m) noexcept {
		assert(IsInside());

		return poll_group.Add(_fd, flags, &m);
	}

	bool ModifyFD(int _fd, unsigned flags, SocketMonitor &m) noexcept {
		assert(IsInside());

		return poll_group.Modify(_fd, flags, &m);
	}

	/**
	 * Remove the given #SocketMonitor after the file descriptor
	 * has been closed.  This is like RemoveFD(), but does not
	 * attempt to use #EPOLL_CTL_DEL.
	 */
	bool Abandon(int fd, SocketMonitor &m) noexcept;

	bool RemoveFD(int fd, SocketMonitor &m) noexcept;

	void AddIdle(IdleMonitor &i) noexcept;
	void RemoveIdle(IdleMonitor &i) noexcept;

	void AddTimer(TimerEvent &t,
		      std::chrono::steady_clock::duration d) noexcept;
	void CancelTimer(TimerEvent &t) noexcept;

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

	/**
	 * The main function of this class.  It will loop until
	 * Break() gets called.  Can be called only once.
	 */
	void Run() noexcept;

private:
	/**
	 * Invoke all pending DeferEvents.
	 *
	 * Caller must lock the mutex.
	 */
	void HandleDeferred() noexcept;

	/**
	 * Invoke all expired #TimerEvent instances and return the
	 * duration until the next timer expires.  Returns a negative
	 * duration if there is no timeout.
	 */
	std::chrono::steady_clock::duration HandleTimers() noexcept;

	bool OnSocketReady(unsigned flags) noexcept override;

public:
	void SetAlive(bool _alive) noexcept {
		alive = _alive;
	}

	bool IsAlive() const noexcept {
		return alive;
	}

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	gcc_pure
	bool IsInside() const noexcept {
		return thread.IsInside();
	}
};

#endif /* MAIN_NOTIFY_H */
