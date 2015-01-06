/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "fs/io/FileOutputStream.hxx"
#include "util/Error.hxx"

#include <unistd.h>
#include <errno.h>
#include <string.h>

static bool
Copy(OutputStream &dest, int src)
{
	Error error;

	while (true) {
		uint8_t buffer[8192];
		ssize_t nbytes = read(src, buffer, sizeof(buffer));
		if (nbytes < 0) {
			fprintf(stderr, "Failed to read from stdin: %s\n",
				strerror(errno));
			return false;
		}

		if (nbytes == 0)
			return true;

		if (!dest.Write(buffer, nbytes, error)) {
			fprintf(stderr, "%s\n", error.GetMessage());
			return false;
		}
	}
}

int
main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: WriteFile PATH\n");
		return EXIT_FAILURE;
	}

	const Path path = Path::FromFS(argv[1]);

	Error error;
	FileOutputStream fos(path, error);
	if (!fos.IsDefined()) {
		fprintf(stderr, "%s\n", error.GetMessage());
		return EXIT_FAILURE;
	}

	if (!Copy(fos, STDIN_FILENO))
		return EXIT_FAILURE;

	if (!fos.Commit(error)) {
		fprintf(stderr, "%s\n", error.GetMessage());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
