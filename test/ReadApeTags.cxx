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

#include "config.h"
#include "tag/ApeLoader.hxx"
#include "thread/Mutex.hxx"
#include "fs/Path.hxx"
#include "fs/NarrowPath.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "util/StringView.hxx"
#include "util/PrintException.hxx"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

static bool
MyApeTagCallback([[maybe_unused]] unsigned long flags,
		 const char *key, StringView value)
{
	if ((flags & (0x3 << 1)) == 0)
		// UTF-8
		printf("\"%s\"=\"%.*s\"\n", key, (int)value.size, value.data);
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
