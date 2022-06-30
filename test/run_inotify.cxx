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
#include "event/InotifyEvent.hxx"
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

struct Instance final : InotifyHandler {
	EventLoop event_loop;
	const ShutdownHandler shutdown_handler{event_loop};

	InotifyEvent inotify_event{event_loop, *this};

	std::exception_ptr error;

	/* virtual methods from class InotifyHandler */
	void OnInotify(int, unsigned mask, const char *name) override {
		printf("mask=0x%x name='%s'\n", mask, name);
	}

	void OnInotifyError(std::exception_ptr _error) noexcept override {
		error = std::move(_error);
		event_loop.Break();
	}
};

int main(int argc, char **argv)
try {
	const char *path;

	if (argc != 2) {
		fprintf(stderr, "Usage: run_inotify PATH\n");
		return EXIT_FAILURE;
	}

	path = argv[1];

	Instance instance;

	instance.inotify_event.AddWatch(path, IN_MASK);

	instance.event_loop.Run();

	if (instance.error)
		std::rethrow_exception(instance.error);

	return EXIT_SUCCESS;
} catch (...) {
	LogError(std::current_exception());
	return EXIT_FAILURE;
}
