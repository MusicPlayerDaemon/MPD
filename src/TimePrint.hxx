// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TIME_PRINT_HXX
#define MPD_TIME_PRINT_HXX

#include <chrono>

class Response;

/**
 * Write a line with a time stamp to the client.
 */
void
time_print(Response &r, const char *name,
	   std::chrono::system_clock::time_point t);

#endif
