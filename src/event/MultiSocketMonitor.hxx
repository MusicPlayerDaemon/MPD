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

#ifndef MPD_MULTI_SOCKET_MONITOR_HXX
#define MPD_MULTI_SOCKET_MONITOR_HXX

#include "check.h"
#include "gcc.h"
#include "glib_compat.h"

#include <glib.h>

#include <forward_list>

#include <assert.h>
#include <stdint.h>

#ifdef WIN32
/* ERRORis a WIN32 macro that poisons our namespace; this is a
   kludge to allow us to use it anyway */
#ifdef ERROR
#undef ERROR
#endif
#endif

class EventLoop;

/**
 * Monitor multiple sockets.
 */
class MultiSocketMonitor {
	struct Source {
		GSource base;

		MultiSocketMonitor *monitor;
	};

	EventLoop &loop;
	Source *source;
	uint64_t absolute_timeout_us;
	std::forward_list<GPollFD> fds;

public:
	static constexpr unsigned READ = G_IO_IN;
	static constexpr unsigned WRITE = G_IO_OUT;
	static constexpr unsigned ERROR = G_IO_ERR;
	static constexpr unsigned HANGUP = G_IO_HUP;

	MultiSocketMonitor(EventLoop &_loop);
	~MultiSocketMonitor();

	EventLoop &GetEventLoop() {
		return loop;
	}

	gcc_pure
	uint64_t GetTime() const {
		return g_source_get_time(&source->base);
	}

	void InvalidateSockets() {
		/* no-op because GLib always calls the GSource's
		   "prepare" method before each poll() anyway */
	}

	void AddSocket(int fd, unsigned events) {
		fds.push_front({fd, gushort(events), 0});
		g_source_add_poll(&source->base, &fds.front());
	}

	template<typename E>
	void UpdateSocketList(E &&e) {
		for (auto prev = fds.before_begin(), end = fds.end(),
			     i = std::next(prev);
		     i != end; i = std::next(prev)) {
			assert(i->events != 0);

			unsigned events = e(i->fd);
			if (events != 0) {
				i->events = events;
				prev = i;
			} else {
				g_source_remove_poll(&source->base, &*i);
				fds.erase_after(prev);
			}
		}
	}

protected:
	/**
	 * @return timeout [ms] or -1 for no timeout
	 */
	virtual int PrepareSockets() = 0;
	virtual void DispatchSockets() = 0;

public:
	/* GSource callbacks */
	static gboolean Prepare(GSource *source, gint *timeout_r);
	static gboolean Check(GSource *source);
	static gboolean Dispatch(GSource *source, GSourceFunc callback,
				 gpointer user_data);

private:
	bool Prepare(gint *timeout_r);
	bool Check() const;

	void Dispatch() {
		DispatchSockets();
	}
};

#endif
