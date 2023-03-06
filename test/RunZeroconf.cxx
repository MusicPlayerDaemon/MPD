// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "event/Loop.hxx"
#include "ShutdownHandler.hxx"
#include "zeroconf/Helper.hxx"

#include <stdlib.h>

int
main([[maybe_unused]] int argc, [[maybe_unused]] char **argv)
{
	EventLoop event_loop;
	const ShutdownHandler shutdown_handler(event_loop);

	const ZeroconfHelper helper(event_loop, "test", "_mpd._tcp", 1234);

	event_loop.Run();

	return EXIT_SUCCESS;
}
