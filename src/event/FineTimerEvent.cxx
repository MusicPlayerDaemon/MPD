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
FineTimerEvent::ScheduleEarlier(Event::Duration d) noexcept
{
	const auto new_due = loop.SteadyNow() + d;

	if (IsPending()) {
		if (new_due >= due)
			return;

		Cancel();
	}

	SetDue(due);
	ScheduleCurrent();
}
