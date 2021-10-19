/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

		event_loop.Break();
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
