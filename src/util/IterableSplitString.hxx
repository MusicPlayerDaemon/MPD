// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "StringSplit.hxx"

#include <iterator>
#include <string_view>

/**
 * Split a string at a certain separator character into sub strings
 * and allow iterating over the segments.
 *
 * Two consecutive separator characters result in an empty string.
 *
 * An empty input string returns one empty string.
 */
template<typename T>
class BasicIterableSplitString {
	using string_view = std::basic_string_view<T>;
	using value_type = typename string_view::value_type;

	string_view s;
	value_type separator;

public:
	constexpr BasicIterableSplitString(string_view _s,
					   value_type _separator) noexcept
		:s(_s), separator(_separator) {}

	class Iterator final {
		friend class BasicIterableSplitString;

		string_view current, rest;

		value_type separator;

		constexpr Iterator(string_view _s,
				   value_type _separator) noexcept
			:rest(_s), separator(_separator)
		{
			Next();
		}

		constexpr Iterator(std::nullptr_t) noexcept
			:current(), rest(), separator() {}

		constexpr void Next() noexcept {
			if (rest.data() == nullptr)
				current = {};
			else {
				const auto [a, b] = Split(rest, separator);
				current = a;
				rest = b;
			}
		}

	public:
		using iterator_category = std::forward_iterator_tag;

		constexpr Iterator &operator++() noexcept{
			Next();
			return *this;
		}

		constexpr bool operator==(Iterator other) const noexcept {
			return current.data() == other.current.data();
		}

		constexpr bool operator!=(Iterator other) const noexcept {
			return !(*this == other);
		}

		constexpr string_view operator*() const noexcept {
			return current;
		}

		constexpr const string_view *operator->() const noexcept {
			return &current;
		}
	};

	using iterator = Iterator;
	using const_iterator = Iterator;

	constexpr const_iterator begin() const noexcept {
		return {s, separator};
	}

	constexpr const_iterator end() const noexcept {
		return nullptr;
	}
};

using IterableSplitString = BasicIterableSplitString<char>;

#ifdef _UNICODE
using WIterableSplitString = BasicIterableSplitString<wchar_t>;
using TIterableSplitString = WIterableSplitString;
#else
using TIterableSplitString = IterableSplitString;
#endif
