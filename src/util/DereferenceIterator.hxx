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
	/* this friend declaration allows the template operator==() to
	   compare arbitrary specializations */
	template<typename, typename>
	friend class DereferenceIterator;

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
		++original;
		return old;
	}

	constexpr auto &operator+=(difference_type n) noexcept {
		original += n;
		return *this;
	}

	constexpr auto operator+(difference_type n) const noexcept {
		return DereferenceIterator{original + n};
	}

	constexpr auto &operator--() noexcept {
		--original;
		return *this;
	}

	constexpr auto operator--(int) noexcept {
		auto old = *this;
		--original;
		return old;
	}

	constexpr auto &operator-=(difference_type n) noexcept {
		original -= n;
		return *this;
	}

	constexpr auto operator-(difference_type n) const noexcept {
		return DereferenceIterator{original - n};
	}

	/* this is a template to allow comparisons with sentinel end
	   iterators */
	template<typename IT2>
	constexpr bool operator==(const DereferenceIterator<IT2, VT> &other) const noexcept {
		return original == other.original;
	}
};

/**
 * A container wrapper that wraps the iterators in a
 * DereferenceIterator.
 */
template<typename CT,
	 typename VT=std::remove_pointer_t<typename std::remove_reference_t<CT>::value_type>>
class DereferenceContainerAdapter {
	CT original;

	/* these aliases allow the underlying container to return a
	   different type for begin() and end() */
	using const_end_iterator = DereferenceIterator<decltype(std::declval<CT>().cend()), const VT>;
	using end_iterator = DereferenceIterator<decltype(std::declval<CT>().end()), VT>;

public:
	using value_type = VT;
	using pointer = VT *;
	using reference = VT &;

	using const_iterator = DereferenceIterator<decltype(std::declval<CT>().cbegin()), const VT>;
	using iterator = DereferenceIterator<decltype(std::declval<CT>().begin()), VT>;

	explicit constexpr DereferenceContainerAdapter(CT &&_original) noexcept
		:original(std::move(_original)) {}

	constexpr iterator begin() noexcept {
		return original.begin();
	}

	constexpr const_iterator begin() const noexcept {
		return original.cbegin();
	}

	constexpr const_iterator cbegin() const noexcept {
		return original.cbegin();
	}

	constexpr end_iterator end() noexcept {
		return original.end();
	}

	constexpr const_end_iterator end() const noexcept {
		return original.cend();
	}

	constexpr const_end_iterator cend() const noexcept {
		return original.cend();
	}
};
