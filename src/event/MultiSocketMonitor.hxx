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

#ifndef MPD_MULTI_SOCKET_MONITOR_HXX
#define MPD_MULTI_SOCKET_MONITOR_HXX

#include "check.h"
#include "IdleMonitor.hxx"
#include "TimeoutMonitor.hxx"
#include "SocketMonitor.hxx"
#include "Compiler.h"

#include <forward_list>
#include <iterator>

#include <assert.h>

#ifdef WIN32
/* ERROR is a WIN32 macro that poisons our namespace; this is a kludge
   to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif
#endif

#ifndef WIN32
struct pollfd;
#endif

class EventLoop;

/**
 * Similar to #SocketMonitor, but monitors multiple sockets.  To use
 * it, implement the methods PrepareSockets() and DispatchSockets().
 * In PrepareSockets(), use UpdateSocketList() and AddSocket().
 * DispatchSockets() will be called if at least one socket is ready.
 */
class MultiSocketMonitor : IdleMonitor, TimeoutMonitor
{
	class SingleFD final : public SocketMonitor {
		MultiSocketMonitor &multi;

		unsigned revents;

	public:
		SingleFD(MultiSocketMonitor &_multi, SocketDescriptor _fd,
			 unsigned events)
			:SocketMonitor(_fd, _multi.GetEventLoop()),
			multi(_multi), revents(0) {
			Schedule(events);
		}

		SocketDescriptor GetFD() const {
			return SocketMonitor::Get();
		}

		unsigned GetEvents() const {
			return SocketMonitor::GetScheduledFlags();
		}

		void SetEvents(unsigned _events) {
			revents &= _events;
			SocketMonitor::Schedule(_events);
		}

		unsigned GetReturnedEvents() const {
			return revents;
		}

		void ClearReturnedEvents() {
			revents = 0;
		}

	protected:
		virtual bool OnSocketReady(unsigned flags) override {
			revents = flags;
			multi.SetReady();
			return true;
		}
	};

	friend class SingleFD;

	/**
	 * DispatchSockets() should be called.
	 */
	bool ready = false;

	/**
	 * PrepareSockets() should be called.
	 *
	 * Note that this doesn't need to be initialized by the
	 * constructor; this class is activated with the first
	 * InvalidateSockets() call, which initializes this flag.
	 */
	bool refresh;

	std::forward_list<SingleFD> fds;

public:
	static constexpr unsigned READ = SocketMonitor::READ;
	static constexpr unsigned WRITE = SocketMonitor::WRITE;
	static constexpr unsigned ERROR = SocketMonitor::ERROR;
	static constexpr unsigned HANGUP = SocketMonitor::HANGUP;

	MultiSocketMonitor(EventLoop &_loop);

	using IdleMonitor::GetEventLoop;

	/**
	 * Clear the socket list and disable all #EventLoop
	 * registrations.  Run this in the #EventLoop thread before
	 * destroying this object.
	 *
	 * Later, this object can be reused and reactivated by calling
	 * InvalidateSockets().
	 *
	 * Note that this class doesn't have a destructor which calls
	 * this method, because this would be racy and thus pointless:
	 * at the time ~MultiSocketMonitor() is called, our virtual
	 * methods have been morphed to be pure again, and in the
	 * meantime the #EventLoop thread could invoke those pure
	 * methods.
	 */
	void Reset();

	/**
	 * Invalidate the socket list.  A call to PrepareSockets() is
	 * scheduled which will then update the list.
	 */
	void InvalidateSockets() {
		refresh = true;
		IdleMonitor::Schedule();
	}

	/**
	 * Add one socket to the list of monitored sockets.
	 *
	 * May only be called from PrepareSockets().
	 */
	void AddSocket(SocketDescriptor fd, unsigned events) {
		fds.emplace_front(*this, fd, events);
	}

	/**
	 * Remove all sockets.
	 *
	 * May only be called from PrepareSockets().
	 */
	void ClearSocketList();

	/**
	 * Update the known sockets by invoking the given function for
	 * each one; its return value is the events bit mask.  A
	 * return value of 0 means the socket will be removed from the
	 * list.
	 *
	 * May only be called from PrepareSockets().
	 */
	template<typename E>
	void UpdateSocketList(E &&e) {
		for (auto prev = fds.before_begin(), end = fds.end(),
			     i = std::next(prev);
		     i != end; i = std::next(prev)) {
			assert(i->GetEvents() != 0);

			unsigned events = e(i->GetFD());
			if (events != 0) {
				i->SetEvents(events);
				prev = i;
			} else {
				fds.erase_after(prev);
			}
		}
	}

#ifndef WIN32
	/**
	 * Replace the socket list with the given file descriptors.
	 * The given pollfd array will be modified by this method.
	 *
	 * May only be called from PrepareSockets().
	 */
	void ReplaceSocketList(pollfd *pfds, unsigned n);
#endif

protected:
	/**
	 * Override this method and update the socket registrations.
	 * To do that, call AddSocket(), ClearSocketList(),
	 * UpdateSocketList() and ReplaceSocketList().
	 *
	 * @return timeout or a negative value for no timeout
	 */
	virtual std::chrono::steady_clock::duration PrepareSockets() = 0;

	/**
	 * At least one socket is ready or the timeout has expired.
	 * This method should be used to perform I/O.
	 */
	virtual void DispatchSockets() = 0;

private:
	void SetReady() {
		ready = true;
		IdleMonitor::Schedule();
	}

	void Prepare();

	virtual void OnTimeout() final {
		SetReady();
		IdleMonitor::Schedule();
	}

	virtual void OnIdle() final;
};

#endif
