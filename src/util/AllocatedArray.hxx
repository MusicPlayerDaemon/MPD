/*
 * Copyright 2010-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ALLOCATED_ARRAY_HXX
#define ALLOCATED_ARRAY_HXX

#include "WritableBuffer.hxx"
#include "Compiler.h"

#include <algorithm>

#include <assert.h>

/**
 * An array allocated on the heap with a length determined at runtime.
 */
template<class T>
class AllocatedArray {
	typedef WritableBuffer<T> Buffer;

public:
	typedef typename Buffer::size_type size_type;
	typedef typename Buffer::reference_type reference_type;
	typedef typename Buffer::const_reference_type const_reference_type;
	typedef typename Buffer::iterator iterator;
	typedef typename Buffer::const_iterator const_iterator;

protected:
	Buffer buffer{nullptr};

public:
	constexpr AllocatedArray() = default;

	explicit AllocatedArray(size_type _size) noexcept
		:buffer{new T[_size], _size} {
		assert(size() == 0 || buffer.data != nullptr);
	}

	explicit AllocatedArray(const AllocatedArray &other) noexcept
		:buffer{new T[other.buffer.size], other.buffer.size} {
		assert(size() == 0 || buffer.data != nullptr);
		assert(other.size() == 0 || other.buffer.data != nullptr);

		std::copy_n(other.buffer.data, buffer.size, buffer.data);
	}

	AllocatedArray(AllocatedArray &&other) noexcept
		:buffer(other.buffer) {
		other.buffer = nullptr;
	}

	~AllocatedArray() noexcept {
		delete[] buffer.data;
	}

	AllocatedArray &operator=(const AllocatedArray &other) noexcept {
		assert(size() == 0 || buffer.data != nullptr);
		assert(other.size() == 0 || other.buffer.data != nullptr);

		if (&other == this)
			return *this;

		ResizeDiscard(other.size());
		std::copy_n(other.buffer.data, other.buffer.size, buffer.data);
		return *this;
	}

	AllocatedArray &operator=(AllocatedArray &&other) noexcept {
		using std::swap;
		swap(buffer, other.buffer);
		return *this;
	}

	constexpr bool IsNull() const noexcept {
		return buffer.IsNull();
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return buffer == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return buffer != nullptr;
	}

	/**
	 * Returns true if no memory was allocated so far.
	 */
	constexpr bool empty() const noexcept {
		return buffer.empty();
	}

	/**
	 * Returns the number of allocated elements.
	 */
	constexpr size_type size() const noexcept {
		return buffer.size;
	}

	reference_type front() noexcept {
		return buffer.front();
	}

	const_reference_type front() const noexcept {
		return buffer.front();
	}

	reference_type back() noexcept {
		return buffer.back();
	}

	const_reference_type back() const noexcept {
		return buffer.back();
	}

	/**
	 * Returns one element.  No bounds checking.
	 */
	reference_type operator[](size_type i) noexcept {
		assert(i < size());

		return buffer.data[i];
	}

	/**
	 * Returns one constant element.  No bounds checking.
	 */
	const_reference_type operator[](size_type i) const noexcept {
		assert(i < size());

		return buffer.data[i];
	}

	iterator begin() noexcept {
		return buffer.begin();
	}

	constexpr const_iterator begin() const noexcept {
		return buffer.cbegin();
	}

	iterator end() noexcept {
		return buffer.end();
	}

	constexpr const_iterator end() const noexcept {
		return buffer.cend();
	}

	/**
	 * Resizes the array, discarding old data.
	 */
	void ResizeDiscard(size_type _size) noexcept {
		if (_size == buffer.size)
			return;

		delete[] buffer.data;
		buffer.size = _size;
		buffer.data = new T[buffer.size];

		assert(size() == 0 || buffer.data != nullptr);
	}

	/**
	 * Grows the array to the specified size, discarding old data.
	 * Similar to ResizeDiscard(), but will never shrink the array to
	 * avoid expensive heap operations.
	 */
	void GrowDiscard(size_type _size) noexcept {
		if (_size > buffer.size)
			ResizeDiscard(_size);
	}

	/**
	 * Grows the array to the specified size, preserving the value of a
	 * range of elements, starting from the beginning.
	 */
	void GrowPreserve(size_type _size, size_type preserve) noexcept {
		if (_size <= buffer.size)
			return;

		T *new_data = new T[_size];
		assert(_size == 0 || new_data != nullptr);

		std::move(buffer.data, buffer.data + preserve, new_data);

		delete[] buffer.data;
		buffer.data = new_data;
		buffer.size = _size;
	}

	/**
	 * Declare that the buffer has the specified size.  Must not be
	 * larger than the current size.  Excess elements are not used (but
	 * they are still allocated).
	 */
	void SetSize(size_type _size) noexcept {
		assert(_size <= buffer.size);

		buffer.size = _size;
	}
};

#endif
