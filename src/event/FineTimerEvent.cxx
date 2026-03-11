// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "FineTimerEvent.hxx"
#include "Loop.hxx"

void
FineTimerEvent::SetDue(Event::Duration d) noexcept
{
	assert(!IsPending());

	SetDue(loop.SteadyNow() + d);
}

void
FineTimerEvent::ScheduleCurrent() noexcept
{
	assert(!IsPending());

	loop.Insert(*this);
}

void
FineTimerEvent::Schedule(Event::Duration d) noexcept
{
	Cancel();

	SetDue(d);
	ScheduleCurrent();
}

void
FineTimerEvent::ScheduleEarlier(Event::TimePoint t) noexcept
{
	if (IsPending()) {
		if (t >= due)
			return;

		Cancel();
	}

	SetDue(t);
	ScheduleCurrent();

}

void
FineTimerEvent::ScheduleEarlier(Event::Duration d) noexcept
{
	ScheduleEarlier(loop.SteadyNow() + d);
}
