// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "io/FileOutputStream.hxx"
#include "fs/NarrowPath.hxx"
#include "util/PrintException.hxx"

#include <cerrno>

#include <unistd.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static bool
Copy(OutputStream &dest, int src)
{
	while (true) {
		std::byte buffer[8192];
		ssize_t nbytes = read(src, buffer, sizeof(buffer));
		if (nbytes < 0) {
			fprintf(stderr, "Failed to read from stdin: %s\n",
				strerror(errno));
			return false;
		}

		if (nbytes == 0)
			return true;

		dest.Write(std::span{buffer}.first(nbytes));
	}
}

int
main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: WriteFile PATH\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath path = argv[1];

	FileOutputStream fos(path);

	if (!Copy(fos, STDIN_FILENO))
		return EXIT_FAILURE;

	fos.Commit();

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
