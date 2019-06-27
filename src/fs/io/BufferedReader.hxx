/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_BUFFERED_READER_HXX
#define MPD_BUFFERED_READER_HXX

#include "util/Compiler.h"
#include "util/DynamicFifoBuffer.hxx"

#include <stddef.h>

class Reader;

class BufferedReader {
	static constexpr size_t MAX_SIZE = 512 * 1024;

	Reader &reader;

	DynamicFifoBuffer<char> buffer;

	bool eof = false;

	unsigned line_number = 0;

public:
	explicit BufferedReader(Reader &_reader) noexcept
		:reader(_reader), buffer(4096) {}

	/**
	 * Reset the internal state.  Should be called after rewinding
	 * the underlying #Reader.
	 */
	void Reset() noexcept {
		buffer.Clear();
		eof = false;
		line_number = 0;
	}

	bool Fill(bool need_more);

	gcc_pure
	WritableBuffer<void> Read() const noexcept {
		return buffer.Read().ToVoid();
	}

	/**
	 * Read a buffer of exactly the given size (without consuming
	 * it).  Throws std::runtime_error if not enough data is
	 * available.
	 */
	void *ReadFull(size_t size);

	void Consume(size_t n) noexcept {
		buffer.Consume(n);
	}

	/**
	 * Read (and consume) data from the input buffer into the
	 * given buffer.  Does not attempt to refill the buffer.
	 */
	size_t ReadFromBuffer(WritableBuffer<void> dest) noexcept;

	/**
	 * Read data into the given buffer and consume it from our
	 * buffer.  Throw an exception if the request cannot be
	 * forfilled.
	 */
	void ReadFull(WritableBuffer<void> dest);

	char *ReadLine();

	unsigned GetLineNumber() const noexcept {
		return line_number;
	}
};

#endif
