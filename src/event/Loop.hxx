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

#ifndef MPD_EVENT_LOOP_HXX
#define MPD_EVENT_LOOP_HXX

#include "check.h"
#include "thread/Id.hxx"
#include "Compiler.h"

#include "PollGroup.hxx"
#include "thread/Mutex.hxx"
#include "WakeFD.hxx"
#include "SocketMonitor.hxx"

#include <chrono>
#include <list>
#include <set>

class TimeoutMonitor;
class IdleMonitor;
class DeferredMonitor;

#include <assert.h>

/**
 * An event loop that polls for events on file/socket descriptors.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs it, except where explicitly documented as
 * thread-safe.
 *
 * @see SocketMonitor, MultiSocketMonitor, TimeoutMonitor, IdleMonitor
 */
class EventLoop final : SocketMonitor
{
	struct TimerRecord {
		/**
		 * Projected monotonic_clock_ms() value when this
		 * timer is due.
		 */
		const std::chrono::steady_clock::time_point due;

		TimeoutMonitor &timer;

		constexpr TimerRecord(TimeoutMonitor &_timer,
				      std::chrono::steady_clock::time_point _due)
			:due(_due), timer(_timer) {}

		bool operator<(const TimerRecord &other) const {
			return due < other.due;
		}

		bool IsDue(std::chrono::steady_clock::time_point _now) const {
			return _now >= due;
		}
	};

	WakeFD wake_fd;

	std::multiset<TimerRecord> timers;
	std::list<IdleMonitor *> idle;

	Mutex mutex;
	std::list<DeferredMonitor *> deferred;

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	bool quit = false;

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

#ifndef NDEBUG
	/**
	 * True if Run() was never called.  This is used for assert()
	 * calls.
	 */
	bool virgin = true;
#endif

	PollGroup poll_group;
	PollResult poll_result;

	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread = ThreadId::Null();

public:
	EventLoop();
	~EventLoop();

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
	void Break();

	bool AddFD(int _fd, unsigned flags, SocketMonitor &m) {
		assert(thread.IsNull() || thread.IsInside());

		return poll_group.Add(_fd, flags, &m);
	}

	bool ModifyFD(int _fd, unsigned flags, SocketMonitor &m) {
		assert(IsInside());

		return poll_group.Modify(_fd, flags, &m);
	}

	/**
	 * Remove the given #SocketMonitor after the file descriptor
	 * has been closed.  This is like RemoveFD(), but does not
	 * attempt to use #EPOLL_CTL_DEL.
	 */
	bool Abandon(int fd, SocketMonitor &m);

	bool RemoveFD(int fd, SocketMonitor &m);

	void AddIdle(IdleMonitor &i);
	void RemoveIdle(IdleMonitor &i);

	void AddTimer(TimeoutMonitor &t,
		      std::chrono::steady_clock::duration d);
	void CancelTimer(TimeoutMonitor &t);

	/**
	 * Schedule a call to DeferredMonitor::RunDeferred().
	 *
	 * This method is thread-safe.
	 */
	void AddDeferred(DeferredMonitor &d);

	/**
	 * Cancel a pending call to DeferredMonitor::RunDeferred().
	 * However after returning, the call may still be running.
	 *
	 * This method is thread-safe.
	 */
	void RemoveDeferred(DeferredMonitor &d);

	/**
	 * The main function of this class.  It will loop until
	 * Break() gets called.  Can be called only once.
	 */
	void Run();

private:
	/**
	 * Invoke all pending DeferredMonitors.
	 *
	 * Caller must lock the mutex.
	 */
	void HandleDeferred();

	virtual bool OnSocketReady(unsigned flags) override;

public:

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	gcc_pure
	bool IsInside() const {
		assert(!thread.IsNull());

		return thread.IsInside();
	}

#ifndef NDEBUG
	gcc_pure
	bool IsInsideOrVirgin() const {
		return virgin || IsInside();
	}
#endif

#ifndef NDEBUG
	gcc_pure
	bool IsInsideOrNull() const {
		return thread.IsNull() || thread.IsInside();
	}
#endif
};

#endif /* MAIN_NOTIFY_H */
