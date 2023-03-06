// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "tag/ApeLoader.hxx"
#include "thread/Mutex.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

static bool
MyApeTagCallback([[maybe_unused]] unsigned long flags,
		 const char *key, std::string_view value)
{
	if ((flags & (0x3 << 1)) == 0)
		// UTF-8
		printf("\"%s\"=\"%.*s\"\n", key,
		       (int)value.size(), value.data());
	else
		printf("\"%s\"=0x%lx\n", key, flags);
	return true;
}

int
main(int argc, char **argv)
try {
#ifdef HAVE_LOCALE_H
	/* initialize locale */
	setlocale(LC_CTYPE,"");
#endif

	if (argc != 2) {
		fprintf(stderr, "Usage: ReadApeTags FILE\n");
		return EXIT_FAILURE;
	}

	const FromNarrowPath path = argv[1];

	Mutex mutex;

	auto is = OpenLocalInputStream(path, mutex);

	if (!tag_ape_scan(*is, MyApeTagCallback)) {
		fprintf(stderr, "error\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
