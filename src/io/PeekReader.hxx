// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PEEK_READER_HXX
#define MPD_PEEK_READER_HXX

#include "Reader.hxx"

#include <cstdint>

/**
 * A filter that allows the caller to peek the first few bytes without
 * consuming them.  The first call must be Peek(), and the following
 * Read() will deliver the same bytes again.
 */
class PeekReader final : public Reader {
	Reader &next;

	size_t buffer_size = 0, buffer_position = 0;

	uint8_t buffer[64];

public:
	explicit PeekReader(Reader &_next)
		:next(_next) {}

	const void *Peek(size_t size);

	/* virtual methods from class Reader */
	size_t Read(void *data, size_t size) override;
};

#endif
