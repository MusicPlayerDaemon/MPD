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
#include "ScopeIOThread.hxx"
#include "storage/Registry.hxx"
#include "storage/StorageInterface.hxx"
#include "storage/FileInfo.hxx"
#include "util/Error.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <memory>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Storage *
MakeStorage(const char *uri)
{
	Error error;
	Storage *storage = CreateStorageURI(io_thread_get(), uri, error);
	if (storage == nullptr) {
		fprintf(stderr, "%s\n", error.GetMessage());
		exit(EXIT_FAILURE);
	}

	return storage;
}

static int
Ls(Storage &storage, const char *path)
{
	Error error;
	auto dir = storage.OpenDirectory(path, error);
	if (dir == nullptr) {
		fprintf(stderr, "%s\n", error.GetMessage());
		return EXIT_FAILURE;
	}

	const char *name;
	while ((name = dir->Read()) != nullptr) {
		FileInfo info;
		if (!dir->GetInfo(false, info, error)) {
			printf("Error on %s: %s\n", name, error.GetMessage());
			error.Clear();
			continue;
		}

		const char *type = "unk";
		switch (info.type) {
		case FileInfo::Type::OTHER:
			type = "oth";
			break;

		case FileInfo::Type::REGULAR:
			type = "reg";
			break;

		case FileInfo::Type::DIRECTORY:
			type = "dir";
			break;
		}

		char mtime[32];
		strftime(mtime, sizeof(mtime), "%F", gmtime(&info.mtime));

		printf("%s %10llu %s %s\n",
		       type, (unsigned long long)info.size,
		       mtime, name);
	}

	delete dir;
	return EXIT_SUCCESS;
}

int
main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "Usage: run_storage COMMAND URI ...\n");
		return EXIT_FAILURE;
	}

	/* initialize GLib */

#ifdef HAVE_GLIB
#if !GLIB_CHECK_VERSION(2,32,0)
	g_thread_init(NULL);
#endif
#endif

	const char *const command = argv[1];
	const char *const storage_uri = argv[2];

	const ScopeIOThread io_thread;

	if (strcmp(command, "ls") == 0) {
		if (argc != 4) {
			fprintf(stderr, "Usage: run_storage ls URI PATH\n");
			return EXIT_FAILURE;
		}

		const char *const path = argv[3];

		std::unique_ptr<Storage> storage(MakeStorage(storage_uri));

		return Ls(*storage, path);
	} else {
		fprintf(stderr, "Unknown command\n");
		return EXIT_FAILURE;
	}
}
