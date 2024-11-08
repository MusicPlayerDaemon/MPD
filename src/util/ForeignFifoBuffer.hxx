// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>

/**
 * A first-in-first-out buffer: you can append data at the end, and
 * read data from the beginning.  This class automatically shifts the
 * buffer as needed.  It is not thread safe.
 *
 * This class does not manage buffer memory.  It will not allocate or
 * free any memory, it only manages the contents of an existing buffer
 * given to the constructor.
 */
template<typename T>
class ForeignFifoBuffer {
public:
	using size_type = std::size_t;
	using Range = std::span<T>;
	using pointer = typename Range::pointer;
	using const_pointer = typename Range::const_pointer;

protected:
	Range buffer;
	size_type head = 0, tail = 0;

public:
	explicit constexpr ForeignFifoBuffer(std::nullptr_t) noexcept
		:buffer() {}

	explicit constexpr ForeignFifoBuffer(Range _buffer) noexcept
		:buffer(_buffer) {}

	constexpr ForeignFifoBuffer(ForeignFifoBuffer &&src) noexcept
		:buffer(src.buffer), head(src.head), tail(src.tail) {
		src.SetNull();
	}

	constexpr ForeignFifoBuffer &operator=(ForeignFifoBuffer &&src) noexcept {
		buffer = src.buffer;
		head = src.head;
		tail = src.tail;
		src.SetNull();
		return *this;
	}

	constexpr void swap(ForeignFifoBuffer<T> &other) noexcept {
		using std::swap;
		swap(buffer, other.buffer);
		swap(head, other.head);
		swap(tail, other.tail);
	}

	friend constexpr void swap(ForeignFifoBuffer<T> &a,
				   ForeignFifoBuffer<T> &b) noexcept {
		a.swap(b);
	}

	constexpr bool IsNull() const noexcept {
		return buffer.data() == nullptr;
	}

	constexpr bool IsDefined() const noexcept {
		return !IsNull();
	}

	T *GetBuffer() noexcept {
		return buffer.data();
	}

	constexpr size_type GetCapacity() const noexcept {
		return buffer.size();
	}

	void SetNull() noexcept {
		buffer = {};
		head = tail = 0;
	}

	void SetBuffer(Range _buffer) noexcept {
		assert(_buffer.data() != nullptr);
		assert(!_buffer.empty());

		buffer = _buffer;
		head = tail = 0;
	}

	void MoveBuffer(Range _buffer) noexcept {
		const auto r = Read();
		assert(_buffer.size() >= r.size());
		std::move(r.begin(), r.end(), _buffer.begin());

		buffer = _buffer;
		tail -= head;
		head = 0;
	}

	constexpr void Clear() noexcept {
		head = tail = 0;
	}

	constexpr bool empty() const noexcept {
		return head == tail;
	}

	constexpr bool IsFull() const noexcept {
		return head == 0 && tail == buffer.size();
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call Append().
	 */
	constexpr Range Write() noexcept {
		if (empty())
			Clear();
		else if (tail == buffer.size())
			Shift();

		return buffer.subspan(tail);
	}

	constexpr bool WantWrite(size_type n) noexcept {
		if (tail + n <= buffer.size())
			/* enough space after the tail */
			return true;

		const size_type in_use = tail - head;
		const size_type required_capacity = in_use + n;
		if (required_capacity > buffer.size())
			return false;

		Shift();
		assert(tail + n <= buffer.size());
		return true;
	}

	/**
	 * Expands the tail of the buffer, after data has been written to
	 * the buffer returned by Write().
	 */
	constexpr void Append(size_type n) noexcept {
		assert(tail <= buffer.size());
		assert(n <= buffer.size());
		assert(tail + n <= buffer.size());

		tail += n;
	}

	constexpr size_type GetAvailable() const noexcept {
		return tail - head;
	}

	/**
	 * Return a buffer range which may be read.  The buffer pointer is
	 * writable, to allow modifications while parsing.
	 */
	constexpr Range Read() const noexcept {
		return buffer.subspan(head, tail - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	constexpr void Consume(size_type n) noexcept {
		assert(tail <= buffer.size());
		assert(head <= tail);
		assert(n <= tail);
		assert(head + n <= tail);

		head += n;
	}

	constexpr size_type Read(pointer p, size_type n) noexcept {
		auto range = Read();
		if (n > range.size())
			n = range.size();
		std::copy_n(range.data(), n, p);
		Consume(n);
		return n;
	}

	/**
	 * Move as much data as possible from the specified buffer.
	 *
	 * @return the number of items moved
	 */
	template<typename U>
	constexpr size_type MoveFrom(std::span<U> src) noexcept {
		auto w = Write();

		if (src.size() > w.size() && head > 0) {
			/* if the source contains more data than we
			   can append at the tail, try to make more
			   room by shifting the head to 0 */
			Shift();
			w = Write();
		}

		if (src.size() > w.size())
			src = src.first(w.size());

		std::move(src.begin(), src.end(), w.begin());
		Append(src.size());
		return src.size();
	}

	constexpr size_type MoveFrom(ForeignFifoBuffer<T> &src) noexcept {
		auto n = MoveFrom(src.Read());
		src.Consume(n);
		return n;
	}

protected:
	constexpr void Shift() noexcept {
		if (head == 0)
			return;

		assert(head <= buffer.size());
		assert(tail <= buffer.size());
		assert(tail >= head);

		const auto r = Read();
		std::move(r.begin(), r.end(), buffer.begin());

		tail -= head;
		head = 0;
	}
};
