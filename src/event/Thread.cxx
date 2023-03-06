// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Thread.hxx"
#include "thread/Name.hxx"
#include "thread/Slack.hxx"
#include "thread/Util.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

static constexpr Domain event_domain("event");

void
EventThread::Start()
{
	assert(!event_loop.IsAlive());
	assert(!thread.IsDefined());

	event_loop.SetAlive(true);

	thread.Start();
}

void
EventThread::Stop() noexcept
{
	if (thread.IsDefined()) {
		assert(event_loop.IsAlive());
		event_loop.SetAlive(false);

		event_loop.InjectBreak();
		thread.Join();
	}
}

void
EventThread::Run() noexcept
{
	SetThreadName(realtime ? "rtio" : "io");

	if (realtime) {
		SetThreadTimerSlack(std::chrono::microseconds(10));

		try {
			SetThreadRealtime();
		} catch (...) {
			FmtInfo(event_domain,
				"RTIOThread could not get realtime scheduling, continuing anyway: {}",
				std::current_exception());
		}
	}

	event_loop.Run();
}
