// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "CoarseTimerEvent.hxx"
#include "Loop.hxx"

void
CoarseTimerEvent::SetDue(Event::Duration d) noexcept
{
	assert(!IsPending());

	SetDue(loop.SteadyNow() + d);
}

void
CoarseTimerEvent::ScheduleCurrent() noexcept
{
	assert(!IsPending());

	loop.Insert(*this);
}

void
CoarseTimerEvent::Schedule(Event::Duration d) noexcept
{
	Cancel();

	SetDue(d);
	ScheduleCurrent();
}

void
CoarseTimerEvent::ScheduleEarlier(Event::Duration d) noexcept
{
	const auto new_due = loop.SteadyNow() + d;

	if (IsPending()) {
		if (new_due >= due)
			return;

		Cancel();
	}

	SetDue(new_due);
	ScheduleCurrent();

}
