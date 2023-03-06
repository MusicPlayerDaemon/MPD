// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_SYNC_STATE_HXX
#define MPD_OGG_SYNC_STATE_HXX

#include <ogg/ogg.h>

#include <cstddef>
#include <cstdint>

class Reader;

/**
 * Wrapper for an ogg_sync_state.
 */
class OggSyncState {
	ogg_sync_state oy;

	Reader &reader;

	/**
	 * Keeps track of the end offset of the most recently returned
	 * page.
	 */
	uint64_t offset = 0;

	/**
	 * The start offset of the most recently returned page.
	 */
	uint64_t start_offset = 0;

public:
	explicit OggSyncState(Reader &_reader)
		:reader(_reader) {
		ogg_sync_init(&oy);
	}

	~OggSyncState() {
		ogg_sync_clear(&oy);
	}

	OggSyncState(const OggSyncState &) = delete;
	OggSyncState &operator=(const OggSyncState &) = delete;

	void Reset() noexcept {
		ogg_sync_reset(&oy);
	}

	void SetOffset(uint64_t _offset) noexcept {
		offset = _offset;
	}

	uint64_t GetStartOffset() const noexcept {
		return start_offset;
	}

	bool Feed(size_t size);

	bool ExpectPage(ogg_page &page);

	bool ExpectPageIn(ogg_stream_state &os);

	bool ExpectPageSeek(ogg_page &page);

	bool ExpectPageSeekIn(ogg_stream_state &os);
};

#endif
