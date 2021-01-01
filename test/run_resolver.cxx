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

#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/ToString.hxx"
#include "net/SocketAddress.hxx"
#include "util/PrintException.hxx"

#include <exception>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: run_resolver HOST\n");
		return EXIT_FAILURE;
	}

	for (const auto &i : Resolve(argv[1], 80, AI_PASSIVE, SOCK_STREAM)) {
		printf("%s\n", ToString(i).c_str());
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
