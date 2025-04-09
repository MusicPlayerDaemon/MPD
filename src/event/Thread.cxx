// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Thread.hxx"
#include "thread/Name.hxx"
#include "thread/Slack.hxx"
#include "thread/Util.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "system/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#ifdef HAVE_URING
#include "util/ScopeExit.hxx"
#include <liburing.h>
#endif

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

	event_loop.SetThread(ThreadId::GetCurrent());

	if (realtime) {
		SetThreadTimerSlack(std::chrono::microseconds(10));

		try {
			SetThreadRealtime();
		} catch (...) {
			FmtInfo(event_domain,
				"RTIOThread could not get realtime scheduling, continuing anyway: {}",
				std::current_exception());
		}
	} else {
#ifdef HAVE_URING
		try {
			try {
				event_loop.EnableUring(1024, IORING_SETUP_SINGLE_ISSUER);
			} catch (const std::system_error &e) {
				if (IsErrno(e, EINVAL))
					/* try without IORING_SETUP_SINGLE_ISSUER
					   (that flag requires Linux kernel 6.0) */
					event_loop.EnableUring(1024, 0);
				else
					throw;
			}
		} catch (...) {
			FmtInfo(event_domain,
				"Failed to initialize io_uring: {}",
				std::current_exception());
		}
#endif
	}

#ifdef HAVE_URING
	AtScopeExit(this) {
		/* make sure that the Uring::Manager gets destructed
		   from within the EventThread, or else its
		   destruction in another thread will cause assertion
		   failures */
		event_loop.DisableUring();
	};
#endif

	event_loop.Run();
}
