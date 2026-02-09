// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "HugeAllocator.hxx"
#include "util/SpanCast.hxx"

#include <utility>

/**
 * Automatic memory management for a dynamic array in "huge" memory.
 */
template<typename T>
class HugeArray {
	using Buffer = std::span<T>;
	Buffer buffer;

public:
	typedef typename Buffer::size_type size_type;
	typedef typename Buffer::value_type value_type;
	typedef typename Buffer::reference reference;
	typedef typename Buffer::const_reference const_reference;
	typedef typename Buffer::iterator iterator;

	constexpr HugeArray() noexcept = default;

	explicit HugeArray(size_type _size)
		:buffer(FromBytesFloor<value_type>(HugeAllocate(sizeof(value_type) * _size))) {}

	constexpr HugeArray(HugeArray &&other) noexcept
		:buffer(std::exchange(other.buffer, nullptr)) {}

	~HugeArray() noexcept {
		if (!buffer.empty()) {
			auto v = std::as_writable_bytes(buffer);
			HugeFree(v.data(), v.size());
		}
	}

	constexpr HugeArray &operator=(HugeArray &&other) noexcept {
		using std::swap;
		swap(buffer, other.buffer);
		return *this;
	}

	void SetName(const char *name) noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeSetName(v.data(), v.size(), name);
	}

	void ForkCow(bool enable) noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeForkCow(v.data(), v.size(), enable);
	}

	void Discard() noexcept {
		const auto v = std::as_writable_bytes(buffer);
		HugeDiscard(v.data(), v.size());
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return buffer == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return buffer != nullptr;
	}

	constexpr operator std::span<T>() noexcept {
		return buffer;
	}

	constexpr operator std::span<const T>() noexcept {
		return buffer;
	}

	/**
	 * Returns the number of allocated elements.
	 */
	constexpr size_type size() const noexcept {
		return buffer.size();
	}

	constexpr reference front() noexcept {
		return buffer.front();
	}

	constexpr const_reference front() const noexcept {
		return buffer.front();
	}

	constexpr reference back() noexcept {
		return buffer.back();
	}

	constexpr const_reference back() const noexcept {
		return buffer.back();
	}

	/**
	 * Returns one element.  No bounds checking.
	 */
	constexpr reference operator[](size_type i) noexcept {
		return buffer[i];
	}

	/**
	 * Returns one constant element.  No bounds checking.
	 */
	constexpr const_reference operator[](size_type i) const noexcept {
		return buffer[i];
	}

	constexpr iterator begin() noexcept {
		return buffer.begin();
	}

	constexpr auto begin() const noexcept {
		return buffer.begin();
	}

	constexpr iterator end() noexcept {
		return buffer.end();
	}

	constexpr auto end() const noexcept {
		return buffer.end();
	}
};
