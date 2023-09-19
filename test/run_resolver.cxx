// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "lib/fmt/SocketAddressFormatter.hxx"
#include "net/Resolver.hxx"
#include "net/AddressInfo.hxx"
#include "net/SocketAddress.hxx"
#include "util/PrintException.hxx"

#include <fmt/format.h>

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
		fmt::print("{}\n", i);
	}

	return EXIT_SUCCESS;
} catch (...) {
	PrintException(std::current_exception());
	return EXIT_FAILURE;
}
