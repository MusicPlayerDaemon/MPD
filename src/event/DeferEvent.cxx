// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
