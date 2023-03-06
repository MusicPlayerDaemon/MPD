// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
