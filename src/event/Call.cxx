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

#include "Call.hxx"
#include "Loop.hxx"
#include "InjectEvent.hxx"
#include "thread/AsyncWaiter.hxx"

#include <cassert>
#include <exception>

class BlockingCallMonitor final
{
	InjectEvent event;

	const std::function<void()> f;

	AsyncWaiter waiter;

public:
	BlockingCallMonitor(EventLoop &_loop,
			    std::function<void()> &&_f) noexcept
		:event(_loop, BIND_THIS_METHOD(RunDeferred)),
		 f(std::move(_f)) {}

	void Run() {
		event.Schedule();
		waiter.Wait();
	}

private:
	void RunDeferred() noexcept {
		try {
			f();
			waiter.SetDone();
		} catch (...) {
			waiter.SetError(std::current_exception());
		}
	}
};

void
BlockingCall(EventLoop &loop, std::function<void()> &&f)
{
	if (!loop.IsAlive() || loop.IsInside()) {
		/* we're already inside the loop - we can simply call
		   the function */
		f();
	} else {
		/* outside the EventLoop's thread - defer execution to
		   the EventLoop, wait for completion */
		BlockingCallMonitor m(loop, std::move(f));
		m.Run();
	}
}
