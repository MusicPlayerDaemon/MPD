// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/xiph/OggSyncState.hxx"
#include "lib/xiph/OggStreamState.hxx"
#include "config/Data.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "input/Reader.hxx"
#include "event/Thread.hxx"
#include "util/PrintException.hxx"

#include <inttypes.h>
#include <stdio.h>

int
main(int argc, char **argv) noexcept
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: DumpOgg FILE\n");
		return EXIT_FAILURE;
	}

	const char *path = argv[1];

	EventThread io_thread;
	io_thread.Start();

	const ScopeInputPluginsInit input_plugins_init(ConfigData(),
						       io_thread.GetEventLoop());

	Mutex mutex;
	auto is = InputStream::OpenReady(path, mutex);

	InputStreamReader reader{*is};

	OggSyncState sync{reader};

	while (true) {
		ogg_page page;
		if (!sync.ExpectPage(page))
			break;

		printf("page offset=%" PRIu64 " serial=%d\n",
		       sync.GetStartOffset(), ogg_page_serialno(&page));
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
