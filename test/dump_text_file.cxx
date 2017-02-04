/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#include "ScopeIOThread.hxx"
#include "input/Init.hxx"
#include "input/InputStream.hxx"
#include "input/TextInputStream.hxx"
#include "config/ConfigGlobal.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <stdexcept>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

class GlobalInit {
	const ScopeIOThread io_thread;

public:
	GlobalInit() {
		config_global_init();
#ifdef ENABLE_ARCHIVE
		archive_plugin_init_all();
#endif
		input_stream_global_init(io_thread_get());
	}

	~GlobalInit() {
		input_stream_global_finish();
#ifdef ENABLE_ARCHIVE
		archive_plugin_deinit_all();
#endif
		config_global_finish();
	}
};

static void
dump_text_file(TextInputStream &is)
{
	const char *line;
	while ((line = is.ReadLine()) != nullptr)
		printf("'%s'\n", line);
}

static int
dump_input_stream(InputStreamPtr &&is)
{
	{
		TextInputStream tis(std::move(is));
		dump_text_file(tis);
	}

	const std::lock_guard<Mutex> protect(is->mutex);

	is->Check();
	return 0;
}

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: run_input URI\n");
		return EXIT_FAILURE;
	}

	/* initialize MPD */

	const GlobalInit init;

	/* open the stream and dump it */

	Mutex mutex;
	Cond cond;

	auto is = InputStream::OpenReady(argv[1], mutex, cond);
	return dump_input_stream(std::move(is));
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
