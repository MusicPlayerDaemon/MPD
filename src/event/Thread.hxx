// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Loop.hxx"
#include "thread/Thread.hxx"

/**
 * A thread which runs an #EventLoop.
 */
class EventThread final {
	EventLoop event_loop{ThreadId::Null()};

	Thread thread{BIND_THIS_METHOD(Run)};

	const bool realtime;

public:
	explicit EventThread(bool _realtime=false)
		:realtime(_realtime) {}

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
