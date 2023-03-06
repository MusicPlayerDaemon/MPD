// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_QUEUE_HXX
#define MPD_DATABASE_QUEUE_HXX

struct Partition;
struct DatabaseSelection;

void
AddFromDatabase(Partition &partition, const DatabaseSelection &selection);

#endif
