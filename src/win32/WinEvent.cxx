// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "WinEvent.hxx"
#include "system/Error.hxx"

WinEvent::WinEvent()
	:event(CreateEventW(nullptr, false, false, nullptr))
{
	if (!event)
		throw MakeLastError("Error creating events");
}
