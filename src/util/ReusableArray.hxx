// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef REUSABLE_ARRAY_HXX
#define REUSABLE_ARRAY_HXX

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
	[[gnu::malloc]] [[gnu::returns_nonnull]]
	T *Get(size_t size) {
		if (size > capacity) [[unlikely]] {
			/* too small: grow */
			delete[] buffer;

			capacity = ((size - 1) | (M - 1)) + 1;
			buffer = new T[capacity];
		}

		return buffer;
	}
};

#endif
