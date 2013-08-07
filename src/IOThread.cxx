/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "thread/Cond.hxx"
#include "event/Loop.hxx"

#include <assert.h>

static struct {
	Mutex mutex;
	Cond cond;

	EventLoop *loop;
	GThread *thread;
} io;

void
io_thread_run(void)
{
	assert(io_thread_inside());
	assert(io.loop != NULL);

	io.loop->Run();
}

static gpointer
io_thread_func(gcc_unused gpointer arg)
{
	/* lock+unlock to synchronize with io_thread_start(), to be
	   sure that io.thread is set */
	io.mutex.lock();
	io.mutex.unlock();

	io_thread_run();
	return NULL;
}

void
io_thread_init(void)
{
	assert(io.loop == NULL);
	assert(io.thread == NULL);

	io.loop = new EventLoop();
}

bool
io_thread_start(gcc_unused GError **error_r)
{
	assert(io.loop != NULL);
	assert(io.thread == NULL);

	const ScopeLock protect(io.mutex);

#if GLIB_CHECK_VERSION(2,32,0)
	io.thread = g_thread_new("io", io_thread_func, nullptr);
#else
	io.thread = g_thread_create(io_thread_func, NULL, true, error_r);
	if (io.thread == NULL)
		return false;
#endif

	return true;
}

void
io_thread_quit(void)
{
	assert(io.loop != NULL);

	io.loop->Break();
}

void
io_thread_deinit(void)
{
	if (io.thread != NULL) {
		io_thread_quit();

		g_thread_join(io.thread);
	}

	delete io.loop;
}

EventLoop &
io_thread_get()
{
	assert(io.loop != nullptr);

	return *io.loop;
}

bool
io_thread_inside(void)
{
	return io.thread != NULL && g_thread_self() == io.thread;
}
