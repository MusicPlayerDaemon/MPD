/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include <glib.h>

#include <type_traits>

#include <assert.h>
#include <stddef.h>

#ifdef WIN32
/* ERRORis a WIN32 macro that poisons our namespace; this is a
   kludge to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif
#endif

class EventLoop;

class SocketMonitor {
	struct Source {
		GSource base;

		SocketMonitor *monitor;
	};

	int fd;
	EventLoop &loop;
	Source *source;
	GPollFD poll;

public:
	static constexpr unsigned READ = G_IO_IN;
	static constexpr unsigned WRITE = G_IO_OUT;
	static constexpr unsigned ERROR = G_IO_ERR;
	static constexpr unsigned HANGUP = G_IO_HUP;

	typedef std::make_signed<size_t>::type ssize_t;

	SocketMonitor(EventLoop &_loop)
		:fd(-1), loop(_loop), source(nullptr) {}

	SocketMonitor(int _fd, EventLoop &_loop);

	~SocketMonitor();

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
	 * and puts the responsibility for closing it to the caller.
	 */
	int Steal();

	void Close();

	void Schedule(unsigned flags) {
		assert(IsDefined());

		poll.events = flags;
		poll.revents &= flags;
		CommitEventFlags();
	}

	void Cancel() {
		poll.events = 0;
		CommitEventFlags();
	}

	void ScheduleRead() {
		poll.events |= READ|HANGUP|ERROR;
		CommitEventFlags();
	}

	void ScheduleWrite() {
		poll.events |= WRITE;
		CommitEventFlags();
	}

	void CancelRead() {
		poll.events &= ~(READ|HANGUP|ERROR);
		CommitEventFlags();
	}

	void CancelWrite() {
		poll.events &= ~WRITE;
		CommitEventFlags();
	}

	ssize_t Read(void *data, size_t length);
	ssize_t Write(const void *data, size_t length);

protected:
	/**
	 * @return false if the socket has been closed
	 */
	virtual bool OnSocketReady(unsigned flags) = 0;

public:
	/* GSource callbacks */
	static gboolean Prepare(GSource *source, gint *timeout_r);
	static gboolean Check(GSource *source);
	static gboolean Dispatch(GSource *source, GSourceFunc callback,
				 gpointer user_data);

private:
	void CommitEventFlags();

	bool Check() const {
		assert(IsDefined());

		return (poll.revents & poll.events) != 0;
	}

	void Dispatch() {
		assert(IsDefined());

		OnSocketReady(poll.revents & poll.events);
	}
};

#endif
