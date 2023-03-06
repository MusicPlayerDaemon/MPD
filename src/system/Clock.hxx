// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_CLOCK_H
#define MPD_CLOCK_H

#ifdef _WIN32

#include <chrono>

/**
 * Returns the uptime of the current process in seconds.
 */
[[gnu::pure]]
std::chrono::seconds
GetProcessUptimeS();

#endif

#endif
