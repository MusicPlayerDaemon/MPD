/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
