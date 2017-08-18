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

#ifndef MPD_EVENT_THREAD_HXX
#define MPD_EVENT_THREAD_HXX

#include "check.h"
#include "Loop.hxx"
#include "thread/Thread.hxx"

/**
 * A thread which runs an #EventLoop.
 */
class EventThread final {
	EventLoop event_loop;

	Thread thread;

public:
	EventThread()
		:event_loop(ThreadId::Null()), thread(BIND_THIS_METHOD(Run)) {}

	~EventThread() {
		Stop();
	}

	EventLoop &GetEventLoop() {
		return event_loop;
	}

	void Start();

	/**
	 * Ask the thread to stop, but does not wait for it.  Usually,
	 * you don't need to call this function, because Stop()
	 * includes this.
	 */
	void StopAsync() {
		event_loop.Break();
	}

	void Stop();

private:
	void Run();
};

#endif /* MAIN_NOTIFY_H */
