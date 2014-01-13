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

#ifndef MPD_SOCKET_MONITOR_HXX
#define MPD_SOCKET_MONITOR_HXX

#include "check.h"
#include "PollGroup.hxx"

#include <type_traits>

#include <assert.h>
#include <stddef.h>

#ifdef WIN32
/* ERROR is a WIN32 macro that poisons our namespace; this is a kludge
   to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif
#endif

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
	int fd;
	EventLoop &loop;

	/**
	 * A bit mask of events that is currently registered in the EventLoop.
	 */
	unsigned scheduled_flags;

public:
	static constexpr unsigned READ = PollGroup::READ;
	static constexpr unsigned WRITE = PollGroup::WRITE;
	static constexpr unsigned ERROR = PollGroup::ERROR;
	static constexpr unsigned HANGUP = PollGroup::HANGUP;

	typedef std::make_signed<size_t>::type ssize_t;

	SocketMonitor(EventLoop &_loop)
		:fd(-1), loop(_loop), scheduled_flags(0) {}

	SocketMonitor(int _fd, EventLoop &_loop)
		:fd(_fd), loop(_loop), scheduled_flags(0) {}

	~SocketMonitor();

	EventLoop &GetEventLoop() {
		return loop;
	}

	bool IsDefined() const {
		return fd >= 0;
	}

	int Get() const {
		assert(IsDefined());

		return fd;
	}

	void Open(int _fd);

	/**
	 * "Steal" the socket descriptor.  This abandons the socket
	 * and returns it.
	 */
	int Steal();

	/**
	 * Somebody has closed the socket.  Unregister this object.
	 */
	void Abandon();

	void Close();

	unsigned GetScheduledFlags() const {
		assert(IsDefined());

		return scheduled_flags;
	}

	void Schedule(unsigned flags);

	void Cancel() {
		Schedule(0);
	}

	void ScheduleRead() {
		Schedule(GetScheduledFlags() | READ | HANGUP | ERROR);
	}

	void ScheduleWrite() {
		Schedule(GetScheduledFlags() | WRITE);
	}

	void CancelRead() {
		Schedule(GetScheduledFlags() & ~(READ|HANGUP|ERROR));
	}

	void CancelWrite() {
		Schedule(GetScheduledFlags() & ~WRITE);
	}

	ssize_t Read(void *data, size_t length);
	ssize_t Write(const void *data, size_t length);

protected:
	/**
	 * @return false if the socket has been closed
	 */
	virtual bool OnSocketReady(unsigned flags) = 0;

public:
	void Dispatch(unsigned flags);
};

#endif
