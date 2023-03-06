// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "CoarseTimerEvent.hxx"
#include "Loop.hxx"

void
CoarseTimerEvent::Schedule(Event::Duration d) noexcept
{
	Cancel();

	due = loop.SteadyNow() + d;
	loop.Insert(*this);
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

	due = new_due;
	loop.Insert(*this);
}
