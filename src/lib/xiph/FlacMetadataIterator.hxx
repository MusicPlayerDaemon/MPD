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

#ifndef MPD_FLAC_METADATA_ITERATOR_HXX
#define MPD_FLAC_METADATA_ITERATOR_HXX

#include "util/Compiler.h"

#include <FLAC/metadata.h>

class FlacMetadataIterator {
	FLAC__Metadata_Iterator *iterator;

public:
	FlacMetadataIterator() noexcept
		:iterator(::FLAC__metadata_iterator_new()) {}

	explicit FlacMetadataIterator(FLAC__Metadata_Chain *chain) noexcept
		:FlacMetadataIterator() {
		::FLAC__metadata_iterator_init(iterator, chain);
	}

	~FlacMetadataIterator() noexcept {
		::FLAC__metadata_iterator_delete(iterator);
	}

	FlacMetadataIterator(const FlacMetadataIterator &) = delete;
	FlacMetadataIterator &operator=(const FlacMetadataIterator &) = delete;

	bool Next() noexcept {
		return ::FLAC__metadata_iterator_next(iterator);
	}

	gcc_pure
	FLAC__StreamMetadata *GetBlock() noexcept {
		return ::FLAC__metadata_iterator_get_block(iterator);
	}
};

#endif
