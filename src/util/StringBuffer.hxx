// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <array>

/**
 * A statically allocated string buffer.
 */
template<typename T, std::size_t CAPACITY>
class BasicStringBuffer {
public:
	using value_type = T;
	using reference = T &;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = std::size_t;

	static constexpr value_type SENTINEL = '\0';

protected:
	using Array = std::array<value_type, CAPACITY>;
	Array the_data;

public:
	using iterator = typename Array::iterator;
	using const_iterator = typename Array::const_iterator;

	static constexpr size_type capacity() noexcept {
		return CAPACITY;
	}

	constexpr bool empty() const noexcept {
		return front() == SENTINEL;
	}

	constexpr void clear() noexcept {
		the_data[0] = SENTINEL;
	}

	constexpr const_pointer c_str() const noexcept {
		return the_data.data();
	}

	constexpr pointer data() noexcept {
		return the_data.data();
	}

	constexpr value_type front() const noexcept {
		return the_data.front();
	}

	/**
	 * Returns one character.  No bounds checking.
	 */
	constexpr value_type operator[](size_type i) const noexcept {
		return the_data[i];
	}

	/**
	 * Returns one writable character.  No bounds checking.
	 */
	constexpr reference operator[](size_type i) noexcept {
		return the_data[i];
	}

	constexpr iterator begin() noexcept {
		return the_data.begin();
	}

	constexpr iterator end() noexcept {
		return the_data.end();
	}

	constexpr const_iterator begin() const noexcept {
		return the_data.begin();
	}

	constexpr const_iterator end() const noexcept {
		return the_data.end();
	}

	constexpr operator const_pointer() const noexcept {
		return c_str();
	}
};

template<std::size_t CAPACITY>
class StringBuffer : public BasicStringBuffer<char, CAPACITY> {};
