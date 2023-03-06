// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "InjectEvent.hxx"
#include "Loop.hxx"

void
InjectEvent::Cancel() noexcept
{
	loop.RemoveInject(*this);
}

void
InjectEvent::Schedule() noexcept
{
	loop.AddInject(*this);
}
