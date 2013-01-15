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

#include <assert.h>

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

	void Close();

	void Schedule(unsigned flags) {
		poll.events = flags;
		poll.revents &= flags;
	}

	void Cancel() {
		poll.events = 0;
	}

	void ScheduleRead() {
		poll.events |= READ|HANGUP|ERROR;
	}

	void ScheduleWrite() {
		poll.events |= WRITE;
	}

	void CancelRead() {
		poll.events &= ~(READ|HANGUP|ERROR);
	}

	void CancelWrite() {
		poll.events &= ~WRITE;
	}

protected:
	virtual void OnSocketReady(unsigned flags) = 0;

public:
	/* GSource callbacks */
	static gboolean Prepare(GSource *source, gint *timeout_r);
	static gboolean Check(GSource *source);
	static gboolean Dispatch(GSource *source, GSourceFunc callback,
				 gpointer user_data);

private:
	bool Check() const {
		return (poll.revents & poll.events) != 0;
	}

	void Dispatch() {
		OnSocketReady(poll.revents & poll.events);
	}
};

#endif
