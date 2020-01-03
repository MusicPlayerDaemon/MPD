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

#ifndef DYNAMIC_FIFO_BUFFER_HXX
#define DYNAMIC_FIFO_BUFFER_HXX

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
		:ForeignFifoBuffer<T>(new T[_capacity], _capacity) {}

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
		ForeignFifoBuffer<T>::MoveBuffer(new_data, new_capacity);
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
		return Write().data;
	}

	/**
	 * Append data to the buffer, growing it as needed.
	 */
	void Append(const_pointer p, size_type n) noexcept {
		std::copy_n(p, n, Write(n));
		Append(n);
	}

protected:
	using ForeignFifoBuffer<T>::GetBuffer;
};

#endif
