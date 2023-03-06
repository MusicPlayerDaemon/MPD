// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
