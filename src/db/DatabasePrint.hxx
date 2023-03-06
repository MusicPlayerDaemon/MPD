// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DB_PRINT_H
#define MPD_DB_PRINT_H

#include <cstdint>
#include <span>

enum TagType : uint8_t;
class SongFilter;
struct DatabaseSelection;
struct Partition;
class Response;

/**
 * @param full print attributes/tags
 * @param base print only base name of songs/directories?
 */
void
db_selection_print(Response &r, Partition &partition,
		   const DatabaseSelection &selection,
		   bool full, bool base);

void
PrintSongUris(Response &r, Partition &partition,
	      const SongFilter *filter);

void
PrintUniqueTags(Response &r, Partition &partition,
		std::span<const TagType> tag_types,
		const SongFilter *filter);

#endif
