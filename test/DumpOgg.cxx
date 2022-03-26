/*
 * Copyright 2003-2022 The Music Player Daemon Project
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
