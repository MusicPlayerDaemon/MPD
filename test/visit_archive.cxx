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
#include "stdbin.h"
#include "tag/Tag.hxx"
#include "config/ConfigGlobal.hxx"
#include "ScopeIOThread.hxx"
#include "input/Init.hxx"
#include "archive/ArchiveList.hxx"
#include "archive/ArchivePlugin.hxx"
#include "archive/ArchiveFile.hxx"
#include "archive/ArchiveVisitor.hxx"
#include "fs/Path.hxx"
#include "util/Error.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <unistd.h>
#include <stdlib.h>

class MyArchiveVisitor final : public ArchiveVisitor {
 public:
	virtual void VisitArchiveEntry(const char *path_utf8) override {
		printf("%s\n", path_utf8);
	}
};

int
main(int argc, char **argv)
{
	Error error;

	if (argc != 3) {
		fprintf(stderr, "Usage: visit_archive PLUGIN PATH\n");
		return EXIT_FAILURE;
	}

	const char *plugin_name = argv[1];
	const Path path = Path::FromFS(argv[2]);

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	/* initialize MPD */

	config_global_init();

	const ScopeIOThread io_thread;

	archive_plugin_init_all();

	if (!input_stream_global_init(error)) {
		fprintf(stderr, "%s", error.GetMessage());
		return 2;
	}

	/* open the archive and dump it */

	const ArchivePlugin *plugin = archive_plugin_from_name(plugin_name);
	if (plugin == nullptr) {
		fprintf(stderr, "No such plugin: %s\n", plugin_name);
		return EXIT_FAILURE;
	}

	int result = EXIT_SUCCESS;

	ArchiveFile *file = archive_file_open(plugin, path, error);
	if (file != nullptr) {
		MyArchiveVisitor visitor;
		file->Visit(visitor);
		file->Close();
	} else {
		fprintf(stderr, "%s", error.GetMessage());
		result = EXIT_FAILURE;
	}

	/* deinitialize everything */

	input_stream_global_finish();

	archive_plugin_deinit_all();

	config_global_finish();

	return result;
}
