// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * This program is a command line interface to SongFilter::Parse().
 *
 */

#include "song/Filter.hxx"
#include "util/PrintException.hxx"

#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc < 2) {
		fprintf(stderr, "Usage: ParseSongFilter FILTER ...\n");
		return 1;
	}

	SongFilter filter;
	filter.Parse({argv + 1, std::size_t(argc - 1)});
	filter.Optimize();

	puts(filter.ToExpression().c_str());
	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
