// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_COUNT_HXX
#define MPD_DB_COUNT_HXX

#include <cstdint>

enum TagType : uint8_t;
struct Partition;
class Response;
class SongFilter;

void
PrintSongCount(Response &r, const Partition &partition, const char *name,
	       const SongFilter *filter,
	       TagType group);

#endif
