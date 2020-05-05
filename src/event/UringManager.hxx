/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#include "SocketMonitor.hxx"
#include "IdleMonitor.hxx"
#include "io/uring/Queue.hxx"

namespace Uring {

class Manager final : public Queue, SocketMonitor, IdleMonitor {
public:
	explicit Manager(EventLoop &event_loop)
		:Queue(1024, 0),
		 SocketMonitor(SocketDescriptor::FromFileDescriptor(GetFileDescriptor()),
			       event_loop),
		 IdleMonitor(event_loop)
	{
		SocketMonitor::ScheduleRead();
	}

	void Push(struct io_uring_sqe &sqe,
		  Operation &operation) noexcept override {
		AddPending(sqe, operation);
		IdleMonitor::Schedule();
	}

private:
	bool OnSocketReady(unsigned flags) noexcept override;
	void OnIdle() noexcept override;
};

} // namespace Uring
