// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>

/**
 * Simple OO wrapper for a const string pointer.
 */
template<typename T=char>
class StringPointer {
public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;

	static constexpr value_type SENTINEL = '\0';

private:
	const_pointer value;

public:
	StringPointer() = default;
	constexpr StringPointer(const_pointer _value) noexcept
		:value(_value) {}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return value == nullptr;
	}

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	constexpr bool IsNull() const noexcept {
		return value == nullptr;
	}

	constexpr const_pointer c_str() const noexcept {
		return value;
	}

	bool empty() const noexcept {
		return *value == SENTINEL;
	}
};
