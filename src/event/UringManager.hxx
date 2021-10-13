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

#pragma once

#include "PipeEvent.hxx"
#include "IdleEvent.hxx"
#include "io/uring/Queue.hxx"

namespace Uring {

class Manager final : public Queue {
	PipeEvent event;
	IdleEvent idle_event;

public:
	explicit Manager(EventLoop &event_loop)
		:Queue(1024, 0),
		 event(event_loop, BIND_THIS_METHOD(OnSocketReady),
		       GetFileDescriptor()),
		 idle_event(event_loop, BIND_THIS_METHOD(OnIdle))
	{
		event.ScheduleRead();
	}

	void Push(struct io_uring_sqe &sqe,
		  Operation &operation) noexcept override {
		AddPending(sqe, operation);
		idle_event.Schedule();
	}

private:
	void OnSocketReady(unsigned flags) noexcept;
	void OnIdle() noexcept;
};

} // namespace Uring
