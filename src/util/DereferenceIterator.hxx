// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <iterator>
#include <type_traits>

/**
 * An iterator wrapper that dereferences the values returned by the
 * original iterator.
 */
template<typename IT,
	 typename VT=std::remove_reference_t<decltype(*std::declval<typename std::iterator_traits<IT>::value_type>())>>
class DereferenceIterator {
	using Traits = std::iterator_traits<IT>;

	IT original;

public:
	using iterator_category = typename Traits::iterator_category;
	using difference_type = typename Traits::difference_type;
	using value_type = VT;
	using pointer = VT *;
	using reference = VT &;

	constexpr DereferenceIterator() = default;

	constexpr DereferenceIterator(const IT _original) noexcept
		:original(_original) {}

	constexpr reference operator*() const noexcept {
		return static_cast<reference>(**original);
	}

	constexpr pointer operator->() const noexcept {
		return static_cast<pointer>(&**original);
	}

	constexpr auto &operator++() noexcept {
		++original;
		return *this;
	}

	constexpr auto operator++(int) noexcept {
		auto old = *this;
		original++;
		return old;
	}

	constexpr auto &operator+=(difference_type n) noexcept {
		original += n;
		return *this;
	}

	constexpr auto &operator+(difference_type n) const noexcept {
		return original + n;
	}

	constexpr auto &operator--() noexcept {
		original = --original;
		return *this;
	}

	constexpr auto operator--(int) noexcept {
		auto old = *this;
		original--;
		return old;
	}

	constexpr auto &operator-=(difference_type n) noexcept {
		original -= n;
		return *this;
	}

	constexpr auto &operator-(difference_type n) const noexcept {
		return original - n;
	}

	constexpr bool operator==(const DereferenceIterator<IT,VT> &other) const noexcept {
		return original == other.original;
	}
};
