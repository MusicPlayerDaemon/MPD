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

#ifndef REUSABLE_ARRAY_HXX
#define REUSABLE_ARRAY_HXX

#include "Compiler.h"

#include <cstddef>
#include <utility>

/**
 * Manager for a temporary array which grows as needed.  This attempts
 * to reduce the number of consecutive heap allocations and
 * deallocations.
 *
 * @param T the array element type
 * @param M always allocate multiples of this number; must be a power of 2
 */
template<typename T, size_t M=1>
class ReusableArray {
	T *buffer = nullptr;
	size_t capacity = 0;

public:
	ReusableArray() = default;

	ReusableArray(ReusableArray &&src)
		:buffer(std::exchange(src.buffer, nullptr)),
		 capacity(std::exchange(src.capacity, 0)) {}

	ReusableArray &operator=(ReusableArray &&src) {
		std::swap(buffer, src.buffer);
		std::swap(capacity, src.capacity);
		return *this;
	}

	~ReusableArray() {
		delete[] buffer;
	}

	size_t GetCapacity() const {
		return capacity;
	}

	/**
	 * Free resources allocated by this object.  This invalidates
	 * the buffer returned by Get().
	 */
	void Clear() {
		delete[] buffer;
		buffer = nullptr;
		capacity = 0;
	}

	/**
	 * Get the buffer, and guarantee a minimum size.  This buffer
	 * becomes invalid with the next Get() call.
	 */
	gcc_malloc gcc_returns_nonnull
	T *Get(size_t size) {
		if (gcc_unlikely(size > capacity)) {
			/* too small: grow */
			delete[] buffer;

			capacity = ((size - 1) | (M - 1)) + 1;
			buffer = new T[capacity];
		}

		return buffer;
	}
};

#endif
