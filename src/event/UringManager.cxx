// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "UringManager.hxx"
#include "util/PrintException.hxx"

namespace Uring {

Manager::Manager(EventLoop &event_loop,
		 unsigned entries, unsigned flags)
	:Queue(entries, flags),
	 event(event_loop, BIND_THIS_METHOD(OnSocketReady),
	       GetFileDescriptor()),
	 idle_event(event_loop, BIND_THIS_METHOD(OnIdle))
{
	event.ScheduleRead();
}

void
Manager::OnSocketReady(unsigned) noexcept
{
	try {
		DispatchCompletions();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

void
Manager::OnIdle() noexcept
{
	try {
		Queue::Submit();
	} catch (...) {
		PrintException(std::current_exception());
	}
}

} // namespace Uring
