// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_STATS_HXX
#define MPD_STATS_HXX

class Response;
struct Partition;

void
stats_invalidate();

void
stats_print(Response &r, const Partition &partition);

#endif
