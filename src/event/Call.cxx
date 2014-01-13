/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include <assert.h>

class BlockingCallMonitor final
	: DeferredMonitor
{
	const std::function<void()> f;

	Mutex mutex;
	Cond cond;

	bool done;

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
	}

private:
	virtual void RunDeferred() override {
		assert(!done);

		f();

		mutex.lock();
		done = true;
		cond.signal();
		mutex.unlock();
	}
};

void
BlockingCall(EventLoop &loop, std::function<void()> &&f)
{
	if (loop.IsInside()) {
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
