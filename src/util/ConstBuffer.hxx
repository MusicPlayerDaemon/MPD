/*
 * Copyright (C) 2013-2017 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef CONST_BUFFER_HXX
#define CONST_BUFFER_HXX

#include "Compiler.h"

#include <cstddef>

#ifndef NDEBUG
#include <assert.h>
#endif

template<typename T>
struct ConstBuffer;

template<>
struct ConstBuffer<void> {
	typedef size_t size_type;
	typedef void value_type;
	typedef const void *pointer_type;
	typedef pointer_type const_pointer_type;
	typedef pointer_type iterator;
	typedef pointer_type const_iterator;

	pointer_type data;
	size_type size;

	ConstBuffer() = default;

	constexpr ConstBuffer(std::nullptr_t):data(nullptr), size(0) {}

	constexpr ConstBuffer(pointer_type _data, size_type _size)
		:data(_data), size(_size) {}

	constexpr static ConstBuffer<void> FromVoid(ConstBuffer<void> other) {
		return other;
	}

	constexpr ConstBuffer<void> ToVoid() const {
		return *this;
	}

	constexpr bool IsNull() const {
		return data == nullptr;
	}

	constexpr bool operator==(std::nullptr_t) const {
		return data == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const {
		return data != nullptr;
	}

	constexpr bool empty() const {
		return size == 0;
	}
};

/**
 * A reference to a memory area that is read-only.
 */
template<typename T>
struct ConstBuffer {
	typedef size_t size_type;
	typedef T value_type;
	typedef const T &reference_type;
	typedef reference_type const_reference_type;
	typedef const T *pointer_type;
	typedef pointer_type const_pointer_type;
	typedef pointer_type iterator;
	typedef pointer_type const_iterator;

	pointer_type data;
	size_type size;

	ConstBuffer() = default;

	constexpr ConstBuffer(std::nullptr_t):data(nullptr), size(0) {}

	constexpr ConstBuffer(pointer_type _data, size_type _size)
		:data(_data), size(_size) {}

	constexpr ConstBuffer(pointer_type _data, pointer_type _end)
		:data(_data), size(_end - _data) {}

	/**
	 * Convert array to ConstBuffer instance.
	 */
	template<size_type _size>
	constexpr ConstBuffer(const T (&_data)[_size])
		:data(_data), size(_size) {}

	/**
	 * Cast a ConstBuffer<void> to a ConstBuffer<T>, rounding down
	 * to the next multiple of T's size.
	 */
	static constexpr ConstBuffer<T> FromVoidFloor(ConstBuffer<void> other) {
		static_assert(sizeof(T) > 0, "Empty base type");
		return ConstBuffer<T>(pointer_type(other.data),
				      other.size / sizeof(T));
	}

	/**
	 * Cast a ConstBuffer<void> to a ConstBuffer<T>.  A "void"
	 * buffer records its size in bytes, and when casting to "T",
	 * the assertion below ensures that the size is a multiple of
	 * sizeof(T).
	 */
#ifdef NDEBUG
	constexpr
#endif
	static ConstBuffer<T> FromVoid(ConstBuffer<void> other) {
		static_assert(sizeof(T) > 0, "Empty base type");
#ifndef NDEBUG
		assert(other.size % sizeof(T) == 0);
#endif
		return FromVoidFloor(other);
	}

	constexpr ConstBuffer<void> ToVoid() const {
		static_assert(sizeof(T) > 0, "Empty base type");
		return ConstBuffer<void>(data, size * sizeof(T));
	}

	constexpr bool IsNull() const {
		return data == nullptr;
	}

	constexpr bool operator==(std::nullptr_t) const {
		return data == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const {
		return data != nullptr;
	}

	constexpr bool empty() const {
		return size == 0;
	}

	template<typename U>
	gcc_pure
	bool Contains(U &&u) const noexcept {
		for (const auto &i : *this)
			if (u == i)
				return true;

		return false;
	}

	constexpr iterator begin() const {
		return data;
	}

	constexpr iterator end() const {
		return data + size;
	}

	constexpr const_iterator cbegin() const {
		return data;
	}

	constexpr const_iterator cend() const {
		return data + size;
	}

#ifdef NDEBUG
	constexpr
#endif
	reference_type operator[](size_type i) const {
#ifndef NDEBUG
		assert(i < size);
#endif

		return data[i];
	}

	/**
	 * Returns a reference to the first element.  Buffer must not
	 * be empty.
	 */
#ifdef NDEBUG
	constexpr
#endif
	reference_type front() const {
#ifndef NDEBUG
		assert(!empty());
#endif
		return data[0];
	}

	/**
	 * Returns a reference to the last element.  Buffer must not
	 * be empty.
	 */
#ifdef NDEBUG
	constexpr
#endif
	reference_type back() const {
#ifndef NDEBUG
		assert(!empty());
#endif
		return data[size - 1];
	}

	/**
	 * Remove the first element (by moving the head pointer, does
	 * not actually modify the buffer).  Buffer must not be empty.
	 */
	void pop_front() {
#ifndef NDEBUG
		assert(!empty());
#endif

		++data;
		--size;
	}

	/**
	 * Remove the last element (by moving the tail pointer, does
	 * not actually modify the buffer).  Buffer must not be empty.
	 */
	void pop_back() {
#ifndef NDEBUG
		assert(!empty());
#endif

		--size;
	}

	/**
	 * Remove the first element and return a reference to it.
	 * Buffer must not be empty.
	 */
	reference_type shift() {
		reference_type result = front();
		pop_front();
		return result;
	}

	void skip_front(size_type n) {
#ifndef NDEBUG
		assert(size >= n);
#endif

		data += n;
		size -= n;
	}

	/**
	 * Move the front pointer to the given address, and adjust the
	 * size attribute to retain the old end address.
	 */
	void MoveFront(pointer_type new_data) {
#ifndef NDEBUG
		assert(IsNull() == (new_data == nullptr));
		assert(new_data <= end());
#endif

		size = end() - new_data;
		data = new_data;
	}

	/**
	 * Move the end pointer to the given address (by adjusting the
	 * size).
	 */
	void SetEnd(pointer_type new_end) {
#ifndef NDEBUG
		assert(IsNull() == (new_end == nullptr));
		assert(new_end >= begin());
#endif

		size = new_end - data;
	}
};

#endif
