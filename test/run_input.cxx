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
#include "TagSave.hxx"
#include "tag/Tag.hxx"
#include "config/ConfigGlobal.hxx"
#include "input/InputStream.hxx"
#include "input/Init.hxx"
#include "event/Thread.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/StdioOutputStream.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#include <stdexcept>

#include <unistd.h>
#include <stdlib.h>

class GlobalInit {
	EventThread io_thread;

public:
	GlobalInit() {
		io_thread.Start();
		config_global_init();
#ifdef ENABLE_ARCHIVE
		archive_plugin_init_all();
#endif
		input_stream_global_init(io_thread.GetEventLoop());
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
tag_save(FILE *file, const Tag &tag)
{
	StdioOutputStream sos(file);
	BufferedOutputStream bos(sos);
	tag_save(bos, tag);
	bos.Flush();
}

static int
dump_input_stream(InputStream *is)
{
	const std::lock_guard<Mutex> protect(is->mutex);

	/* print meta data */

	if (is->HasMimeType())
		fprintf(stderr, "MIME type: %s\n", is->GetMimeType());

	/* read data and tags from the stream */

	while (!is->IsEOF()) {
		Tag *tag = is->ReadTag();
		if (tag != NULL) {
			fprintf(stderr, "Received a tag:\n");
			tag_save(stderr, *tag);
			delete tag;
		}

		char buffer[4096];
		size_t num_read = is->Read(buffer, sizeof(buffer));
		if (num_read == 0)
			break;

		ssize_t num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

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
	return dump_input_stream(is.get());
} catch (const std::exception &e) {
	LogError(e);
	return EXIT_FAILURE;
}
