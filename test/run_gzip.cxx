// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
		std::byte buffer[4096];
		ssize_t nbytes = read(src, buffer, sizeof(buffer));
		if (nbytes <= 0) {
			if (nbytes < 0)
				throw MakeErrno("read() failed");

			return;
		}

		dest.Write(std::span{buffer}.first(nbytes));
	}
}

static void
CopyGzip(OutputStream &_dest, int src)
{
	GzipOutputStream dest(_dest);
	Copy(dest, src);
	dest.Finish();
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
