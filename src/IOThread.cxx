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
#include "IOThread.hxx"
#include "thread/Mutex.hxx"
#include "thread/Thread.hxx"
#include "thread/Name.hxx"
#include "event/Loop.hxx"

#include <assert.h>

static struct {
	Mutex mutex;

	EventLoop *loop;
	Thread thread;
} io;

static void
io_thread_run(void)
{
	assert(io_thread_inside());
	assert(io.loop != nullptr);

	io.loop->Run();
}

static void
io_thread_func(gcc_unused void *arg)
{
	SetThreadName("io");

	/* lock+unlock to synchronize with io_thread_start(), to be
	   sure that io.thread is set */
	io.mutex.lock();
	io.mutex.unlock();

	io_thread_run();
}

void
io_thread_init(void)
{
	assert(io.loop == nullptr);
	assert(!io.thread.IsDefined());

	io.loop = new EventLoop();
}

void
io_thread_start()
{
	assert(io.loop != nullptr);
	assert(!io.thread.IsDefined());

	const std::lock_guard<Mutex> protect(io.mutex);
	io.thread.Start(io_thread_func, nullptr);
}

void
io_thread_quit(void)
{
	assert(io.loop != nullptr);

	io.loop->Break();
}

void
io_thread_deinit(void)
{
	if (io.thread.IsDefined()) {
		io_thread_quit();
		io.thread.Join();
	}

	delete io.loop;
}

EventLoop &
io_thread_get()
{
	assert(io.loop != nullptr);

	return *io.loop;
}
