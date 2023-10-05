// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/zlib/GunzipReader.hxx"
#include "io/FileReader.hxx"
#include "io/StdioOutputStream.hxx"
#include "fs/NarrowPath.hxx"
#include "util/PrintException.hxx"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
Copy(OutputStream &dest, Reader &src)
{
	while (true) {
		std::byte buffer[4096];
		std::size_t nbytes = src.Read(buffer);
		if (nbytes == 0)
			break;

		dest.Write(std::span{buffer}.first(nbytes));
	}
}

static void
CopyGunzip(OutputStream &dest, Reader &_src)
{
	GunzipReader src(_src);
	Copy(dest, src);
}

static void
CopyGunzip(FILE *_dest, Path src_path)
{
	StdioOutputStream dest(_dest);
	FileReader src(src_path);
	CopyGunzip(dest, src);
}

int
main(int argc, [[maybe_unused]] char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: run_gunzip PATH\n");
		return EXIT_FAILURE;
	}

	FromNarrowPath path = argv[1];

	CopyGunzip(stdout, path);
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
