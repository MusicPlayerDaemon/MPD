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
	size_type head = 0, tail = 0, capacity;
	T *data;

public:
	explicit constexpr ForeignFifoBuffer(std::nullptr_t n) noexcept
		:capacity(0), data(n) {}

	constexpr ForeignFifoBuffer(T *_data, size_type _capacity) noexcept
		:capacity(_capacity), data(_data) {}

	constexpr ForeignFifoBuffer(ForeignFifoBuffer &&src) noexcept
		:head(src.head), tail(src.tail),
		 capacity(src.capacity), data(src.data) {
		src.SetNull();
	}

	constexpr ForeignFifoBuffer &operator=(ForeignFifoBuffer &&src) noexcept {
		head = src.head;
		tail = src.tail;
		capacity = src.capacity;
		data = src.data;
		src.SetNull();
		return *this;
	}

	constexpr void swap(ForeignFifoBuffer<T> &other) noexcept {
		using std::swap;
		swap(head, other.head);
		swap(tail, other.tail);
		swap(capacity, other.capacity);
		swap(data, other.data);
	}

	friend constexpr void swap(ForeignFifoBuffer<T> &a,
				   ForeignFifoBuffer<T> &b) noexcept {
		a.swap(b);
	}

	constexpr bool IsNull() const noexcept {
		return data == nullptr;
	}

	constexpr bool IsDefined() const noexcept {
		return !IsNull();
	}

	T *GetBuffer() noexcept {
		return data;
	}

	constexpr size_type GetCapacity() const noexcept {
		return capacity;
	}

	void SetNull() noexcept {
		head = tail = 0;
		capacity = 0;
		data = nullptr;
	}

	void SetBuffer(T *_data, size_type _capacity) noexcept {
		assert(_data != nullptr);
		assert(_capacity > 0);

		head = tail = 0;
		capacity = _capacity;
		data = _data;
	}

	void MoveBuffer(T *new_data, size_type new_capacity) noexcept {
		assert(new_capacity >= tail - head);

		std::move(data + head, data + tail, new_data);
		data = new_data;
		capacity = new_capacity;
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
		return head == 0 && tail == capacity;
	}

	/**
	 * Prepares writing.  Returns a buffer range which may be written.
	 * When you are finished, call Append().
	 */
	constexpr Range Write() noexcept {
		if (empty())
			Clear();
		else if (tail == capacity)
			Shift();

		return Range(data + tail, capacity - tail);
	}

	constexpr bool WantWrite(size_type n) noexcept {
		if (tail + n <= capacity)
			/* enough space after the tail */
			return true;

		const size_type in_use = tail - head;
		const size_type required_capacity = in_use + n;
		if (required_capacity > capacity)
			return false;

		Shift();
		assert(tail + n <= capacity);
		return true;
	}

	/**
	 * Expands the tail of the buffer, after data has been written to
	 * the buffer returned by Write().
	 */
	constexpr void Append(size_type n) noexcept {
		assert(tail <= capacity);
		assert(n <= capacity);
		assert(tail + n <= capacity);

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
		return Range(data + head, tail - head);
	}

	/**
	 * Marks a chunk as consumed.
	 */
	constexpr void Consume(size_type n) noexcept {
		assert(tail <= capacity);
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

		assert(head <= capacity);
		assert(tail <= capacity);
		assert(tail >= head);

		std::move(data + head, data + tail, data);

		tail -= head;
		head = 0;
	}
};
