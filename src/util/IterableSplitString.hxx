/*
 * Copyright 2013-2022 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StringSplit.hxx"
#include "StringView.hxx"

#include <iterator>

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

	using StringView = BasicStringView<T>;

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

		constexpr StringView operator*() const noexcept {
			return current;
		}

		constexpr const StringView *operator->() const noexcept {
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
