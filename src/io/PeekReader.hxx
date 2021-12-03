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
