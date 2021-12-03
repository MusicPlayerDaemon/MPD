/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
		uint8_t buffer[8192];
		ssize_t nbytes = read(src, buffer, sizeof(buffer));
		if (nbytes < 0) {
			fprintf(stderr, "Failed to read from stdin: %s\n",
				strerror(errno));
			return false;
		}

		if (nbytes == 0)
			return true;

		dest.Write(buffer, nbytes);
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
