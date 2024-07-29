// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cassert>
#include <cstddef>
#include <span>

/**
 * A circular buffer.
 *
 * This class does not manage buffer memory.  It will not allocate or
 * free any memory, it only manages the contents of an existing
 * buffer given to the constructor.
 *
 * Everything between #head and #tail is valid data (may wrap around).
 * If both are equal, then the buffer is empty.  Due to this
 * implementation detail, the buffer is empty when #size-1 items are
 * stored; the last buffer cell cannot be used.
 */
template<typename T>
class CircularBuffer {
public:
	using Range = std::span<T>;
	using pointer = typename Range::pointer;
	using size_type = typename Range::size_type;

protected:
	/**
	 * The next index to be read.
	 */
	size_type head = 0;

	/**
	 * The next index to be written to.
	 */
	size_type tail = 0;

	const std::span<T> buffer;

public:
	explicit constexpr CircularBuffer(Range _buffer) noexcept
		:buffer(_buffer) {}

	CircularBuffer(const CircularBuffer &other) = delete;
	CircularBuffer &operator=(const CircularBuffer &other) = delete;

protected:
	constexpr size_type Next(size_type i) const noexcept {
		return i + 1 == buffer.size()
			? 0
			: i + 1;
	}

public:
	constexpr void Clear() noexcept {
		head = tail = 0;
	}

	constexpr size_type GetCapacity() const noexcept {
		return buffer.size();
	}

	constexpr bool empty() const noexcept {
		return head == tail;
	}

	constexpr bool IsFull() const noexcept {
		return Next(tail) == head;
	}

	/**
	 * Returns the number of elements stored in this buffer.
	 */
	constexpr size_type GetSize() const noexcept {
		return head <= tail
			? tail - head
			: buffer.size() - head + tail;
	}

	/**
	 * Returns the number of elements that can be added to this
	 * buffer.
	 */
	constexpr size_type GetSpace() const noexcept {
		/* space = capacity - size - 1 */
		return (head <= tail
			? buffer.size() - tail + head
			: head - tail)
			- 1;
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call Append().
	 */
	constexpr Range Write() noexcept {
		assert(head < buffer.size());
		assert(tail < buffer.size());

		size_type end = tail < head
			? head - 1
			/* the "head==0" is there so we don't write
			   the last cell, as this situation cannot be
			   represented by head/tail */
			: buffer.size() - (head == 0);

		return buffer.subspan(tail, end - tail);
	}

	/**
	 * Expands the tail of the buffer, after data has been written
	 * to the buffer returned by Write().
	 */
	constexpr void Append(size_type n) noexcept {
		assert(head < buffer.size());
		assert(tail < buffer.size());
		assert(n < buffer.size());
		assert(tail + n <= buffer.size());
		assert(head <= tail || tail + n < head);

		tail += n;

		if (tail == buffer.size()) {
			assert(head > 0);
			tail = 0;
		}
	}

	/**
	 * Return a buffer range which may be read.  The buffer pointer is
	 * writable, to allow modifications while parsing.
	 */
	constexpr Range Read() noexcept {
		assert(head < buffer.size());
		assert(tail < buffer.size());

		return buffer.subspan(head, (tail < head ? buffer.size() : tail) - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	constexpr void Consume(size_type n) noexcept {
		assert(head < buffer.size());
		assert(tail < buffer.size());
		assert(n < buffer.size());
		assert(head + n <= buffer.size());
		assert(tail < head || head + n <= tail);

		head += n;
		if (head == buffer.size())
			head = 0;
	}
};
