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

#include "ShutdownHandler.hxx"
#include "db/update/InotifySource.hxx"
#include "event/Loop.hxx"
#include "Log.hxx"

#include <exception>

#include <sys/inotify.h>

static constexpr unsigned IN_MASK =
#ifdef IN_ONLYDIR
	IN_ONLYDIR|
#endif
	IN_ATTRIB|IN_CLOSE_WRITE|IN_CREATE|IN_DELETE|IN_DELETE_SELF
	|IN_MOVE|IN_MOVE_SELF;

static void
my_inotify_callback([[maybe_unused]] int wd, unsigned mask,
		    const char *name, [[maybe_unused]] void *ctx)
{
	printf("mask=0x%x name='%s'\n", mask, name);
}

int main(int argc, char **argv)
try {
	const char *path;

	if (argc != 2) {
		fprintf(stderr, "Usage: run_inotify PATH\n");
		return EXIT_FAILURE;
	}

	path = argv[1];

	EventLoop event_loop;
	const ShutdownHandler shutdown_handler(event_loop);

	InotifySource source(event_loop, my_inotify_callback, nullptr);
	source.Add(path, IN_MASK);

	event_loop.Run();

	return EXIT_SUCCESS;
} catch (...) {
	LogError(std::current_exception());
	return EXIT_FAILURE;
}
