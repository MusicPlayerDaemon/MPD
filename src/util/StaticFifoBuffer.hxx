/*
 * Copyright 2003-2019 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STATIC_FIFO_BUFFER_HXX
#define STATIC_FIFO_BUFFER_HXX

#include "WritableBuffer.hxx"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

/**
 * A first-in-first-out buffer: you can append data at the end, and
 * read data from the beginning.  This class automatically shifts the
 * buffer as needed.  It is not thread safe.
 */
template<class T, size_t size>
class StaticFifoBuffer {
public:
	using size_type = std::size_t;
	using Range = WritableBuffer<T>;

protected:
	size_type head = 0, tail = 0;
	T data[size];

public:
	constexpr size_type GetCapacity() const noexcept {
		return size;
	}

	void Shift() noexcept {
		if (head == 0)
			return;

		assert(head <= size);
		assert(tail <= size);
		assert(tail >= head);

		std::move(data + head, data + tail, data);

		tail -= head;
		head = 0;
	}

	void Clear() noexcept {
		head = tail = 0;
	}

	constexpr bool empty() const noexcept {
		return head == tail;
	}

	constexpr bool IsFull() const noexcept {
		return head == 0 && tail == size;
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call Append().
	 */
	Range Write() noexcept {
		if (empty())
			Clear();
		else if (tail == size)
			Shift();

		return Range(data + tail, size - tail);
	}

	/**
	 * Expands the tail of the buffer, after data has been written to
	 * the buffer returned by Write().
	 */
	void Append(size_type n) noexcept {
		assert(tail <= size);
		assert(n <= size);
		assert(tail + n <= size);

		tail += n;
	}

	constexpr size_type GetAvailable() const noexcept {
		return tail - head;
	}

	/**
	 * Return a buffer range which may be read.  The buffer pointer is
	 * writable, to allow modifications while parsing.
	 */
	constexpr Range Read() noexcept {
		return Range(data + head, tail - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	void Consume(size_type n) noexcept {
		assert(tail <= size);
		assert(head <= tail);
		assert(n <= tail);
		assert(head + n <= tail);

		head += n;
	}
};

#endif
