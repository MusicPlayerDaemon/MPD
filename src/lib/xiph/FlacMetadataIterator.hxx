// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FLAC_METADATA_ITERATOR_HXX
#define MPD_FLAC_METADATA_ITERATOR_HXX

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

	[[gnu::pure]]
	FLAC__StreamMetadata *GetBlock() noexcept {
		return ::FLAC__metadata_iterator_get_block(iterator);
	}
};

#endif
