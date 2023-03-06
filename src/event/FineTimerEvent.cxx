// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FineTimerEvent.hxx"
#include "Loop.hxx"

void
FineTimerEvent::Schedule(Event::Duration d) noexcept
{
	Cancel();

	due = loop.SteadyNow() + d;
	loop.Insert(*this);
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

	due = new_due;
	loop.Insert(*this);
}
