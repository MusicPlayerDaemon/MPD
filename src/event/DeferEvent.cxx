// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "DeferEvent.hxx"
#include "Loop.hxx"

void
DeferEvent::Schedule() noexcept
{
	if (!IsPending())
		loop.AddDefer(*this);

	assert(IsPending());
}

void
DeferEvent::ScheduleIdle() noexcept
{
	if (!IsPending())
		loop.AddIdle(*this);

	assert(IsPending());
}

void
DeferEvent::ScheduleNext() noexcept
{
	if (!IsPending())
		loop.AddNext(*this);

	assert(IsPending());
}
