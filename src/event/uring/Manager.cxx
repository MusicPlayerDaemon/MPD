// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Manager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

Manager::Manager(EventLoop &event_loop,
		 unsigned entries, unsigned flags)
	:Queue(entries, flags),
	 event(event_loop, BIND_THIS_METHOD(OnReady), GetFileDescriptor())
{
	event.ScheduleRead();
}

Manager::Manager(EventLoop &event_loop,
		 unsigned entries, struct io_uring_params &params)
	:Queue(entries, params),
	 event(event_loop, BIND_THIS_METHOD(OnReady), GetFileDescriptor())
{
	event.ScheduleRead();
}

void
Manager::Submit()
{
	/* defer in "idle" mode to allow accumulation of more
	   events */
	defer_submit_event.ScheduleIdle();
}

void
Manager::DispatchCompletions() noexcept
{
	try {
		Queue::DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}

	CheckVolatileEvent();
}

inline void
Manager::OnReady(unsigned) noexcept
{
	DispatchCompletions();
}

void
Manager::DeferredSubmit() noexcept
{
	try {
		Queue::Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
