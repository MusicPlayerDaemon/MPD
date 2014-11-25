/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "stdbin.h"
#include "tag/Tag.hxx"
#include "config/ConfigGlobal.hxx"
#include "input/InputStream.hxx"
#include "input/Init.hxx"
#include "ScopeIOThread.hxx"
#include "util/Error.hxx"
#include "thread/Cond.hxx"
#include "Log.hxx"
#include "fs/io/BufferedOutputStream.hxx"
#include "fs/io/StdioOutputStream.hxx"

#ifdef ENABLE_ARCHIVE
#include "archive/ArchiveList.hxx"
#endif

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <unistd.h>
#include <stdlib.h>

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
	is->Lock();

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

		Error error;
		char buffer[4096];
		size_t num_read = is->Read(buffer, sizeof(buffer), error);
		if (num_read == 0) {
			if (error.IsDefined())
				LogError(error);

			break;
		}

		ssize_t num_written = write(1, buffer, num_read);
		if (num_written <= 0)
			break;
	}

	Error error;
	if (!is->Check(error)) {
		LogError(error);
		is->Unlock();
		return EXIT_FAILURE;
	}

	is->Unlock();

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: run_input URI\n");
		return EXIT_FAILURE;
	}

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	/* initialize MPD */

	config_global_init();

	const ScopeIOThread io_thread;

#ifdef ENABLE_ARCHIVE
	archive_plugin_init_all();
#endif

	Error error;
	if (!input_stream_global_init(error)) {
		LogError(error);
		return 2;
	}

	/* open the stream and dump it */

	Mutex mutex;
	Cond cond;

	InputStream *is = InputStream::OpenReady(argv[1], mutex, cond, error);
	int ret;
	if (is != NULL) {
		ret = dump_input_stream(is);
		delete is;
	} else {
		if (error.IsDefined())
			LogError(error);
		else
			fprintf(stderr, "input_stream::Open() failed\n");
		ret = EXIT_FAILURE;
	}

	/* deinitialize everything */

	input_stream_global_finish();

#ifdef ENABLE_ARCHIVE
	archive_plugin_deinit_all();
#endif

	config_global_finish();

	return ret;
}
