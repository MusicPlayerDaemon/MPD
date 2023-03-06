// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MaskMonitor.hxx"

void
MaskMonitor::OrMask(unsigned new_mask) noexcept
{
	if (pending_mask.fetch_or(new_mask) == 0)
		event.Schedule();
}

void
MaskMonitor::RunDeferred() noexcept
{
	const unsigned mask = pending_mask.exchange(0);
	if (mask != 0)
		callback(mask);
}
