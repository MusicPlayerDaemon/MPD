/*
 * Copyright (C) 2013-2016 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ITERABLE_SPLIT_STRING_HXX
#define ITERABLE_SPLIT_STRING_HXX

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
	typedef BasicStringView<T> StringView;

	using value_type = typename StringView::value_type;

	StringView s;
	value_type separator;

public:
	constexpr BasicIterableSplitString(StringView _s,
					   value_type _separator)
		:s(_s), separator(_separator) {}

	class Iterator final {
		friend class BasicIterableSplitString;

		StringView current, rest;

		value_type separator;

		Iterator(StringView _s, value_type _separator)
			:rest(_s), separator(_separator) {
			Next();
		}

		constexpr Iterator(std::nullptr_t n)
			:current(n), rest(n), separator(0) {}

		void Next() {
			if (rest == nullptr)
				current = nullptr;
			else {
				const auto *i = rest.Find(separator);
				if (i == nullptr) {
					current = rest;
					rest.data = nullptr;
				} else {
					current.data = rest.data;
					current.size = i - current.data;
					rest.size -= current.size + 1;
					rest.data = i + 1;
				}
			}
		}

	public:
		using iterator_category = std::forward_iterator_tag;

		Iterator &operator++() {
			Next();
			return *this;
		}

		constexpr bool operator==(Iterator other) const {
			return current.data == other.current.data;
		}

		constexpr bool operator!=(Iterator other) const {
			return !(*this == other);
		}

		constexpr StringView operator*() const {
			return current;
		}

		constexpr const StringView *operator->() const {
			return &current;
		}
	};

	using iterator = Iterator;
	using const_iterator = Iterator;

	const_iterator begin() const {
		return {s, separator};
	}

	constexpr const_iterator end() const {
		return {nullptr};
	}
};

using IterableSplitString = BasicIterableSplitString<char>;

#ifdef _UNICODE
using WIterableSplitString = BasicIterableSplitString<wchar_t>;
using TIterableSplitString = WIterableSplitString;
#else
using TIterableSplitString = IterableSplitString;
#endif

#endif
