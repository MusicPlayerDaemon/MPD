/*
 * Copyright 2014-2019 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BUFFERED_READER_HXX
#define BUFFERED_READER_HXX

#include "util/DynamicFifoBuffer.hxx"

#include <cstddef>

class Reader;

class BufferedReader {
	static constexpr std::size_t MAX_SIZE = 512 * 1024;

	Reader &reader;

	DynamicFifoBuffer<char> buffer;

	bool eof = false;

	unsigned line_number = 0;

public:
	explicit BufferedReader(Reader &_reader) noexcept
		:reader(_reader), buffer(16384) {}

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

	[[gnu::pure]]
	WritableBuffer<void> Read() const noexcept {
		return buffer.Read().ToVoid();
	}

	/**
	 * Read a buffer of exactly the given size (without consuming
	 * it).  Throws std::runtime_error if not enough data is
	 * available.
	 */
	void *ReadFull(std::size_t size);

	void Consume(std::size_t n) noexcept {
		buffer.Consume(n);
	}

	/**
	 * Read (and consume) data from the input buffer into the
	 * given buffer.  Does not attempt to refill the buffer.
	 */
	std::size_t ReadFromBuffer(WritableBuffer<void> dest) noexcept;

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
