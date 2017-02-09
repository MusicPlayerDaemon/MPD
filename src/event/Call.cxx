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

#include "config.h"
#include "Call.hxx"
#include "Loop.hxx"
#include "DeferredMonitor.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "Compiler.h"

#include <exception>

#include <assert.h>

class BlockingCallMonitor final
	: DeferredMonitor
{
	const std::function<void()> f;

	Mutex mutex;
	Cond cond;

	bool done;

	std::exception_ptr exception;

public:
	BlockingCallMonitor(EventLoop &_loop, std::function<void()> &&_f)
		:DeferredMonitor(_loop), f(std::move(_f)), done(false) {}

	void Run() {
		assert(!done);

		Schedule();

		mutex.lock();
		while (!done)
			cond.wait(mutex);
		mutex.unlock();

		if (exception)
			std::rethrow_exception(exception);
	}

private:
	virtual void RunDeferred() override {
		assert(!done);

		try {
			f();
		} catch (...) {
			exception = std::current_exception();
		}

		mutex.lock();
		done = true;
		cond.signal();
		mutex.unlock();
	}
};

void
BlockingCall(EventLoop &loop, std::function<void()> &&f)
{
	if (loop.IsInsideOrNull()) {
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
