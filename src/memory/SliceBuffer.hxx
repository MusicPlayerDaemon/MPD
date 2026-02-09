// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SLICE_BUFFER_HXX
#define MPD_SLICE_BUFFER_HXX

#include "HugeAllocator.hxx"

#include <cassert>
#include <cstddef>
#include <new>
#include <utility>

/**
 * This class pre-allocates a certain number of objects, and allows
 * callers to allocate and free these objects ("slices").
 */
template<typename T>
class SliceBuffer {
	union Slice {
		Slice *next;

		T value;
	};

	HugeArray<Slice> buffer;

	/**
	 * The number of slices that are initialized.  This is used to
	 * avoid page faulting on the new allocation, so the kernel
	 * does not need to reserve physical memory pages.
	 */
	unsigned n_initialized = 0;

	/**
	 * The number of slices currently allocated.
	 */
	unsigned n_allocated = 0;

	/**
	 * Pointer to the first free element in the chain.
	 */
	Slice *available = nullptr;

public:
	SliceBuffer(unsigned _count)
		:buffer(_count) {
		buffer.ForkCow(false);
	}

	~SliceBuffer() noexcept {
		/* all slices must be freed explicitly, and this
		   assertion checks for leaks */
		assert(n_allocated == 0);
	}

	SliceBuffer(const SliceBuffer &other) = delete;
	SliceBuffer &operator=(const SliceBuffer &other) = delete;

	unsigned GetCapacity() const noexcept {
		return buffer.size();
	}

	bool empty() const noexcept {
		return n_allocated == 0;
	}

	bool IsFull() const noexcept {
		return n_allocated == buffer.size();
	}

	void SetName(const char *name) noexcept {
		buffer.SetName(name);
	}

	void DiscardMemory() noexcept {
		assert(empty());

		n_initialized = 0;
		buffer.Discard();
		available = nullptr;
	}

	template<typename... Args>
	T *Allocate(Args&&... args) {
		assert(n_initialized <= buffer.size());
		assert(n_allocated <= n_initialized);

		if (available == nullptr) {
			if (n_initialized == buffer.size()) {
				/* out of (internal) memory, buffer is full */
				assert(n_allocated == buffer.size());
				return nullptr;
			}

			available = &buffer[n_initialized++];
			available->next = nullptr;
		}

		/* allocate a slice */
		T *value = &available->value;
		available = available->next;
		++n_allocated;

		/* construct the object */
		return ::new((void *)value) T(std::forward<Args>(args)...);
	}

	void Free(T *value) noexcept {
		assert(n_initialized <= buffer.size());
		assert(n_allocated > 0);
		assert(n_allocated <= n_initialized);

		Slice *slice = reinterpret_cast<Slice *>(value);
		assert(slice >= &buffer.front() && slice <= &buffer.back());

		/* destruct the object */
		value->~T();

		/* insert the slice in the "available" linked list */
		slice->next = available;
		available = slice;
		--n_allocated;

		/* give memory back to the kernel when the last slice
		   was freed */
		if (n_allocated == 0) {
			DiscardMemory();
		}
	}
};

#endif
