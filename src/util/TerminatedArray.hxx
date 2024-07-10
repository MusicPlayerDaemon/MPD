// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <iterator> // for std::iterator_traits

/**
 * A container-like class which allows iterating over an array
 * terminated by sentinel value.  Most commonly, this is an array of
 * pointers terminated by nullptr, but null-terminated C strings can
 * also be used.
 *
 * @param T the type of an item in the aray
 * @param Sentinel the sentinel value
 */
template<typename T, T Sentinel>
class TerminatedArray {
	T *head;

	/**
	 * A special type for the end iterator which is always equal
	 * the iterator that points to the #Sentinel value.
	 */
	struct sentinel {
		using Traits = std::iterator_traits<T *>;

		using iterator_category = typename Traits::iterator_category;
		using difference_type = typename Traits::difference_type;
		using value_type = typename Traits::value_type;
		using pointer = typename Traits::pointer;
		using reference = typename Traits::reference;
	};

public:
	using value_type = T;

	explicit constexpr TerminatedArray(T *_head) noexcept
		:head(_head) {}

	class iterator {
		using Traits = std::iterator_traits<T *>;

		T *cursor;

	public:
		using iterator_category = typename Traits::iterator_category;
		using difference_type = typename Traits::difference_type;
		using value_type = typename Traits::value_type;
		using pointer = typename Traits::pointer;
		using reference = typename Traits::reference;

		explicit constexpr iterator(T *_cursor) noexcept
			:cursor(_cursor) {}

		constexpr bool operator==(const iterator &other) const noexcept {
			return cursor == other.cursor;
		}

		constexpr bool operator==(const sentinel &) const noexcept {
			return *cursor == Sentinel;
		}

		constexpr auto &operator++() noexcept {
			++cursor;
			return *this;
		}

		constexpr auto operator++(int) noexcept {
			auto old = *this;
			++cursor;
			return old;
		}

		constexpr auto &operator+=(difference_type n) noexcept {
			cursor += n;
			return *this;
		}

		constexpr auto operator+(difference_type n) const noexcept {
			return iterator{cursor + n};
		}

		constexpr auto &operator--() noexcept {
			--cursor;
			return *this;
		}

		constexpr auto operator--(int) noexcept {
			auto old = *this;
			--cursor;
			return old;
		}

		constexpr auto &operator-=(difference_type n) noexcept {
			cursor -= n;
			return *this;
		}

		constexpr auto operator-(difference_type n) const noexcept {
			return iterator{cursor - n};
		}

		reference operator*() const noexcept {
			return *cursor;
		}

		pointer operator->() const noexcept {
			return cursor;
		}
	};

	constexpr iterator begin() const noexcept {
		return iterator{head};
	}

	constexpr sentinel end() const noexcept {
		return {};
	}

	constexpr iterator cbegin() const noexcept {
		return begin();
	}

	constexpr sentinel cend() const noexcept {
		return end();
	}
};
