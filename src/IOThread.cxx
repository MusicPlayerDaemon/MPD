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
#include "event/Thread.hxx"

#include <assert.h>

static EventThread *io_thread;

void
io_thread_init(void)
{
	assert(io_thread == nullptr);

	io_thread = new EventThread();
}

void
io_thread_start()
{
	assert(io_thread != nullptr);

	io_thread->Start();
}

void
io_thread_deinit(void)
{
	if (io_thread == nullptr)
		return;

	io_thread->Stop();
	delete io_thread;
	io_thread = nullptr;
}
