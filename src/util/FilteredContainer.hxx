// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <iterator> // for std::iterator_traits

/**
 * A container wrapper which returns only elements for which a second
 * container returns true.
 */
template<std::input_iterator AB, typename AE,
	 std::input_iterator BB>
class FilteredContainer {
	[[no_unique_address]]
	AB a_begin;

	[[no_unique_address]]
	AE a_end;

	[[no_unique_address]]
	BB b_begin;

	struct end_iterator {
		using Traits = std::iterator_traits<AE>;

		using iterator_category = typename Traits::iterator_category;
		using difference_type = typename Traits::difference_type;
		using value_type = typename Traits::value_type;
		using pointer = typename Traits::pointer;
		using reference = typename Traits::reference;

		AE a;
	};

public:
	explicit constexpr FilteredContainer(AB _a_begin, AE _a_end, BB _b_begin) noexcept
		:a_begin(_a_begin), a_end(_a_end), b_begin(_b_begin) {}

	class iterator {
		using Traits = std::iterator_traits<AB>;

		[[no_unique_address]]
		AB a;

		[[no_unique_address]]
		AE a_end;

		[[no_unique_address]]
		BB b;

	public:
		using iterator_category = typename std::forward_iterator_tag;
		using difference_type = typename Traits::difference_type;
		using value_type = typename Traits::value_type;
		using pointer = typename Traits::pointer;
		using reference = typename Traits::reference;

		explicit constexpr iterator(AB _a, AE _a_end, BB _b) noexcept
			:a(_a), a_end(_a_end), b(_b) {}

		constexpr void FindEnabled() noexcept {
			while (a != a_end && !*b) {
				++a;
				++b;
			}
		}

		constexpr bool operator==(const iterator &other) const noexcept {
			return a == other.a;
		}

		constexpr bool operator==(const end_iterator &other) const noexcept {
			return a == other.a;
		}

		constexpr auto &operator++() noexcept {
			++a;
			++b;
			FindEnabled();
			return *this;
		}

		constexpr auto operator++(int) noexcept {
			auto old = *this;
			++*this;
			return old;
		}

		reference operator*() const noexcept {
			return a.operator*();
		}

		pointer operator->() const noexcept {
			return a.operator->();
		}
	};

	constexpr iterator begin() const noexcept {
		iterator it{a_begin, a_end, b_begin};
		it.FindEnabled();
		return it;
	}

	constexpr end_iterator end() const noexcept {
		return {a_end};
	}

	constexpr iterator cbegin() const noexcept {
		return begin();
	}

	constexpr end_iterator cend() const noexcept {
		return end();
	}
};
