// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "ForeignFifoBuffer.hxx"

/**
 * A first-in-first-out buffer: you can append data at the end, and
 * read data from the beginning.  This class automatically shifts the
 * buffer as needed.  It is not thread safe.
 */
template<typename T>
class DynamicFifoBuffer : protected ForeignFifoBuffer<T> {
public:
	using typename ForeignFifoBuffer<T>::size_type;
	using typename ForeignFifoBuffer<T>::pointer;
	using typename ForeignFifoBuffer<T>::const_pointer;
	using typename ForeignFifoBuffer<T>::Range;

	/**
	 * Construct without allocating a buffer.
	 */
	explicit constexpr DynamicFifoBuffer(std::nullptr_t n) noexcept
		:ForeignFifoBuffer<T>(n) {}

	/**
	 * Allocate a buffer with the given capacity.
	 */
	explicit DynamicFifoBuffer(size_type _capacity) noexcept
		:ForeignFifoBuffer<T>(std::span{new T[_capacity], _capacity}) {}

	~DynamicFifoBuffer() noexcept {
		delete[] GetBuffer();
	}

	DynamicFifoBuffer(const DynamicFifoBuffer &) = delete;

	using ForeignFifoBuffer<T>::GetCapacity;
	using ForeignFifoBuffer<T>::Clear;
	using ForeignFifoBuffer<T>::empty;
	using ForeignFifoBuffer<T>::IsFull;
	using ForeignFifoBuffer<T>::GetAvailable;
	using ForeignFifoBuffer<T>::Read;
	using ForeignFifoBuffer<T>::Consume;
	using ForeignFifoBuffer<T>::Write;
	using ForeignFifoBuffer<T>::Append;

	void Grow(size_type new_capacity) noexcept {
		assert(new_capacity > GetCapacity());

		T *old_data = GetBuffer();
		T *new_data = new T[new_capacity];
		ForeignFifoBuffer<T>::MoveBuffer({new_data, new_capacity});
		delete[] old_data;
	}

	void WantWrite(size_type n) noexcept {
		if (ForeignFifoBuffer<T>::WantWrite(n))
			/* we already have enough space */
			return;

		const size_type in_use = GetAvailable();
		const size_type required_capacity = in_use + n;
		size_type new_capacity = GetCapacity();
		do {
			new_capacity <<= 1;
		} while (new_capacity < required_capacity);

		Grow(new_capacity);
	}

	/**
	 * Write data to the buffer, growing it as needed.  Returns a
	 * writable pointer.
	 */
	pointer Write(size_type n) noexcept {
		WantWrite(n);
		return Write().data();
	}

	/**
	 * Append data to the buffer, growing it as needed.
	 */
	void Append(std::span<const T> src) noexcept {
		std::copy(src.begin(), src.end(), Write(src.size()));
		Append(src.size());
	}

protected:
	using ForeignFifoBuffer<T>::GetBuffer;
};
