/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include <list>
#include <set>

class TimeoutMonitor;
class IdleMonitor;
class DeferredMonitor;
class SocketMonitor;

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
		const unsigned due_ms;

		TimeoutMonitor &timer;

		constexpr TimerRecord(TimeoutMonitor &_timer,
				      unsigned _due_ms)
			:due_ms(_due_ms), timer(_timer) {}

		bool operator<(const TimerRecord &other) const {
			return due_ms < other.due_ms;
		}

		bool IsDue(unsigned _now_ms) const {
			return _now_ms >= due_ms;
		}
	};

	WakeFD wake_fd;

	std::multiset<TimerRecord> timers;
	std::list<IdleMonitor *> idle;

	Mutex mutex;
	std::list<DeferredMonitor *> deferred;

	unsigned now_ms;

	bool quit;

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
	bool busy;

#ifndef NDEBUG
	/**
	 * True if Run() was never called.  This is used for assert()
	 * calls.
	 */
	bool virgin;
#endif

	PollGroup poll_group;
	PollResult poll_result;

	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread;

public:
	EventLoop();
	~EventLoop();

	/**
	 * A caching wrapper for MonotonicClockMS().
	 */
	unsigned GetTimeMS() const {
		assert(IsInside());

		return now_ms;
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

	void AddTimer(TimeoutMonitor &t, unsigned ms);
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
