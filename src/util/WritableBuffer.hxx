/*
 * Copyright 2013-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef WRITABLE_BUFFER_HXX
#define WRITABLE_BUFFER_HXX

#include "ConstBuffer.hxx"

#include <cassert>
#include <cstddef>

template<typename T>
struct WritableBuffer;

template<>
struct WritableBuffer<void> {
	typedef std::size_t size_type;
	using value_type = void;
	using pointer = void *;
	using const_pointer = const void *;
	using iterator = pointer;
	using const_iterator = const_pointer;

	pointer data;
	size_type size;

	WritableBuffer() = default;

	constexpr WritableBuffer(std::nullptr_t) noexcept
		:data(nullptr), size(0) {}

	constexpr WritableBuffer(pointer _data, size_type _size) noexcept
		:data(_data), size(_size) {}

	constexpr static WritableBuffer<void> FromVoid(WritableBuffer<void> other) noexcept {
		return other;
	}

	constexpr WritableBuffer<void> ToVoid() const noexcept {
		return *this;
	}

	constexpr operator ConstBuffer<void>() const noexcept {
		return {data, size};
	}

	constexpr bool IsNull() const noexcept {
		return data == nullptr;
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return data == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return data != nullptr;
	}

	constexpr bool empty() const noexcept {
		return size == 0;
	}
};

/**
 * A reference to a memory area that is writable.
 *
 * @see ConstBuffer
 */
template<typename T>
struct WritableBuffer {
	using size_type = std::size_t;
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using iterator = pointer;
	using const_iterator = const_pointer;

	pointer data;
	size_type size;

	WritableBuffer() = default;

	constexpr WritableBuffer(std::nullptr_t) noexcept
		:data(nullptr), size(0) {}

	constexpr WritableBuffer(pointer _data, size_type _size) noexcept
		:data(_data), size(_size) {}

	constexpr WritableBuffer(pointer _data, pointer _end) noexcept
		:data(_data), size(_end - _data) {}

	/**
	 * Convert array to WritableBuffer instance.
	 */
	template<size_type _size>
	constexpr WritableBuffer(T (&_data)[_size]) noexcept
		:data(_data), size(_size) {}

	constexpr operator ConstBuffer<T>() const noexcept {
		return {data, size};
	}

	/**
	 * Cast a WritableBuffer<void> to a WritableBuffer<T>,
	 * rounding down to the next multiple of T's size.
	 */
	static constexpr WritableBuffer<T> FromVoidFloor(WritableBuffer<void> other) noexcept {
		static_assert(sizeof(T) > 0, "Empty base type");
		return WritableBuffer<T>(pointer(other.data),
					 other.size / sizeof(T));
	}

	/**
	 * Cast a WritableBuffer<void> to a WritableBuffer<T>.  A "void"
	 * buffer records its size in bytes, and when casting to "T",
	 * the assertion below ensures that the size is a multiple of
	 * sizeof(T).
	 */
	static constexpr WritableBuffer<T> FromVoid(WritableBuffer<void> other) noexcept {
		static_assert(sizeof(T) > 0, "Empty base type");
		assert(other.size % sizeof(T) == 0);
		return FromVoidFloor(other);
	}

	constexpr WritableBuffer<void> ToVoid() const noexcept {
		static_assert(sizeof(T) > 0, "Empty base type");
		return WritableBuffer<void>(data, size * sizeof(T));
	}

	constexpr bool IsNull() const noexcept {
		return data == nullptr;
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return data == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return data != nullptr;
	}

	constexpr bool empty() const noexcept {
		return size == 0;
	}

	constexpr iterator begin() const noexcept {
		return data;
	}

	constexpr iterator end() const noexcept {
		return data + size;
	}

	constexpr const_iterator cbegin() const noexcept {
		return data;
	}

	constexpr const_iterator cend() const noexcept {
		return data + size;
	}

	constexpr reference operator[](size_type i) const noexcept {
		assert(i < size);

		return data[i];
	}

	/**
	 * Returns a reference to the first element.  Buffer must not
	 * be empty.
	 */
	constexpr reference front() const noexcept {
		assert(!empty());
		return data[0];
	}

	/**
	 * Returns a reference to the last element.  Buffer must not
	 * be empty.
	 */
	constexpr reference back() const noexcept {
		assert(!empty());
		return data[size - 1];
	}

	/**
	 * Remove the first element (by moving the head pointer, does
	 * not actually modify the buffer).  Buffer must not be empty.
	 */
	constexpr void pop_front() noexcept {
		assert(!empty());

		++data;
		--size;
	}

	/**
	 * Remove the last element (by moving the tail pointer, does
	 * not actually modify the buffer).  Buffer must not be empty.
	 */
	constexpr void pop_back() noexcept {
		assert(!empty());

		--size;
	}

	/**
	 * Remove the first element and return a reference to it.
	 * Buffer must not be empty.
	 */
	constexpr reference shift() noexcept {
		reference result = front();
		pop_front();
		return result;
	}

	constexpr void skip_front(size_type n) noexcept {
		assert(size >= n);

		data += n;
		size -= n;
	}

	/**
	 * Move the front pointer to the given address, and adjust the
	 * size attribute to retain the old end address.
	 */
	void MoveFront(pointer new_data) noexcept {
		assert(IsNull() == (new_data == nullptr));
		assert(new_data <= end());

		size = end() - new_data;
		data = new_data;
	}

	/**
	 * Move the end pointer to the given address (by adjusting the
	 * size).
	 */
	void SetEnd(pointer new_end) noexcept {
		assert(IsNull() == (new_end == nullptr));
		assert(new_end >= begin());

		size = new_end - data;
	}
};

#endif
