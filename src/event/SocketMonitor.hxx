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

#ifndef MPD_SOCKET_MONITOR_HXX
#define MPD_SOCKET_MONITOR_HXX

#include "PollGroup.hxx"
#include "net/SocketDescriptor.hxx"

#include <type_traits>

#include <assert.h>
#include <stddef.h>

class EventLoop;

/**
 * Monitor events on a socket.  Call Schedule() to announce events
 * you're interested in, or Cancel() to cancel your subscription.  The
 * #EventLoop will invoke virtual method OnSocketReady() as soon as
 * any of the subscribed events are ready.
 *
 * This class does not feel responsible for closing the socket.  Call
 * Close() to do it manually.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class SocketMonitor {
	SocketDescriptor fd = SocketDescriptor::Undefined();
	EventLoop &loop;

	/**
	 * A bit mask of events that is currently registered in the EventLoop.
	 */
	unsigned scheduled_flags = 0;

public:
	static constexpr unsigned READ = PollGroup::READ;
	static constexpr unsigned WRITE = PollGroup::WRITE;
	static constexpr unsigned ERROR = PollGroup::ERROR;
	static constexpr unsigned HANGUP = PollGroup::HANGUP;

	typedef std::make_signed<size_t>::type ssize_t;

	explicit SocketMonitor(EventLoop &_loop) noexcept
		:loop(_loop) {}

	SocketMonitor(SocketDescriptor _fd, EventLoop &_loop) noexcept
		:fd(_fd), loop(_loop) {}

	~SocketMonitor() noexcept;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsDefined() const noexcept {
		return fd.IsDefined();
	}

	SocketDescriptor GetSocket() const noexcept {
		assert(IsDefined());

		return fd;
	}

	void Open(SocketDescriptor _fd) noexcept;

	/**
	 * "Steal" the socket descriptor.  This abandons the socket
	 * and returns it.
	 */
	SocketDescriptor Steal() noexcept;

	void Close() noexcept;

	unsigned GetScheduledFlags() const noexcept {
		assert(IsDefined());

		return scheduled_flags;
	}

	void Schedule(unsigned flags) noexcept;

	void Cancel() noexcept {
		Schedule(0);
	}

	void ScheduleRead() noexcept {
		Schedule(GetScheduledFlags() | READ | HANGUP | ERROR);
	}

	void ScheduleWrite() noexcept {
		Schedule(GetScheduledFlags() | WRITE);
	}

	void CancelRead() noexcept {
		Schedule(GetScheduledFlags() & ~(READ|HANGUP|ERROR));
	}

	void CancelWrite() noexcept {
		Schedule(GetScheduledFlags() & ~WRITE);
	}

protected:
	/**
	 * @return false if the socket has been closed
	 */
	virtual bool OnSocketReady(unsigned flags) noexcept = 0;

public:
	void Dispatch(unsigned flags) noexcept;
};

#endif
