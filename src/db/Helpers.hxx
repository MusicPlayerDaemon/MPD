// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_HELPERS_HXX
#define MPD_DATABASE_HELPERS_HXX

class Database;
struct DatabaseSelection;
struct DatabaseStats;

DatabaseStats
GetStats(const Database &db, const DatabaseSelection &selection);

#endif
