// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_EVENT_THREAD_HXX
#define MPD_EVENT_THREAD_HXX

#include "Loop.hxx"
#include "thread/Thread.hxx"

/**
 * A thread which runs an #EventLoop.
 */
class EventThread final {
	EventLoop event_loop;

	Thread thread;

	const bool realtime;

public:
	explicit EventThread(bool _realtime=false)
		:event_loop(ThreadId::Null()), thread(BIND_THIS_METHOD(Run)),
		 realtime(_realtime) {}

	~EventThread() noexcept {
		Stop();
	}

	EventLoop &GetEventLoop() noexcept {
		return event_loop;
	}

	void Start();

	void Stop() noexcept;

private:
	void Run() noexcept;
};

#endif /* MAIN_NOTIFY_H */
