// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "db/plugins/simple/DatabaseSave.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/io/TextFile.hxx"
#include "util/PrintException.hxx"

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: LoadDatabase PATH\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath db_path = argv[1];

	Directory root{{}, nullptr};
	TextFile line_reader{db_path};
	db_load_internal(line_reader, root, true);

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
