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

#include "lib/zlib/GzipOutputStream.hxx"
#include "io/StdioOutputStream.hxx"
#include "system/Error.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
Copy(OutputStream &dest, int src)
{
	while (true) {
		char buffer[4096];
		ssize_t nbytes = read(src, buffer, sizeof(buffer));
		if (nbytes <= 0) {
			if (nbytes < 0)
				throw MakeErrno("read() failed");

			return;
		}

		dest.Write(buffer, nbytes);
	}
}

static void
CopyGzip(OutputStream &_dest, int src)
{
	GzipOutputStream dest(_dest);
	Copy(dest, src);
	dest.Flush();
}

static void
CopyGzip(FILE *_dest, int src)
{
	StdioOutputStream dest(_dest);
	CopyGzip(dest, src);
}

int
main(int argc, [[maybe_unused]] char **argv)
try {
	if (argc != 1) {
		fprintf(stderr, "Usage: run_gzip\n");
		return EXIT_FAILURE;
	}

	CopyGzip(stdout, STDIN_FILENO);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
