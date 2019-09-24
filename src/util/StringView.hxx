/*
 * Copyright 2013-2019 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_VIEW_HXX
#define STRING_VIEW_HXX

#include "ConstBuffer.hxx"
#include "StringAPI.hxx"
#include "Compiler.h"

#include <utility>

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
#include <string_view>
#endif

template<typename T>
struct BasicStringView : ConstBuffer<T> {
	typedef typename ConstBuffer<T>::size_type size_type;
	typedef typename ConstBuffer<T>::value_type value_type;
	typedef typename ConstBuffer<T>::pointer_type pointer_type;

	using ConstBuffer<T>::data;
	using ConstBuffer<T>::size;

	BasicStringView() = default;

	explicit constexpr BasicStringView(ConstBuffer<T> src)
		:ConstBuffer<T>(src) {}

	explicit constexpr BasicStringView(ConstBuffer<void> src)
		:ConstBuffer<T>(ConstBuffer<T>::FromVoid(src)) {}

	constexpr BasicStringView(pointer_type _data, size_type _size) noexcept
		:ConstBuffer<T>(_data, _size) {}

	constexpr BasicStringView(pointer_type _begin,
				  pointer_type _end) noexcept
		:ConstBuffer<T>(_begin, _end - _begin) {}

	BasicStringView(pointer_type _data) noexcept
		:ConstBuffer<T>(_data,
				_data != nullptr ? StringLength(_data) : 0) {}

	constexpr BasicStringView(std::nullptr_t n) noexcept
		:ConstBuffer<T>(n) {}

#if __cplusplus >= 201703L && !GCC_OLDER_THAN(7,0)
	constexpr BasicStringView(std::basic_string_view<T> src) noexcept
		:ConstBuffer<T>(src.data(), src.size()) {}

	constexpr operator std::basic_string_view<T>() const noexcept {
		return {data, size};
	}
#endif

	using ConstBuffer<T>::empty;
	using ConstBuffer<T>::begin;
	using ConstBuffer<T>::end;
	using ConstBuffer<T>::front;
	using ConstBuffer<T>::back;
	using ConstBuffer<T>::pop_front;
	using ConstBuffer<T>::pop_back;
	using ConstBuffer<T>::skip_front;

	gcc_pure
	pointer_type Find(value_type ch) const noexcept {
		return StringFind(data, ch, this->size);
	}

	gcc_pure
	pointer_type FindLast(value_type ch) const noexcept {
		return StringFindLast(data, ch, size);
	}

	/**
	 * Split the string at the first occurrence of the given
	 * character.  If the character is not found, then the first
	 * value is the whole string and the second value is nullptr.
	 */
	gcc_pure
	std::pair<BasicStringView<T>, BasicStringView<T>> Split(value_type ch) const noexcept {
		const auto separator = Find(ch);
		if (separator == nullptr)
			return {*this, nullptr};

		return {{begin(), separator}, {separator + 1, end()}};
	}

	gcc_pure
	bool StartsWith(BasicStringView<T> needle) const noexcept {
		return this->size >= needle.size &&
			StringIsEqual(data, needle.data, needle.size);
	}

	gcc_pure
	bool EndsWith(BasicStringView<T> needle) const noexcept {
		return this->size >= needle.size &&
			StringIsEqual(data + this->size - needle.size,
				      needle.data, needle.size);
	}

	gcc_pure
	int Compare(BasicStringView<T> other) const noexcept {
		if (size < other.size) {
			int result = StringCompare(data, other.data, size);
			if (result == 0)
				result = -1;
			return result;
		} else if (size > other.size) {
			int result = StringCompare(data, other.data,
						   other.size);
			if (result == 0)
				result = 1;
			return result;
		} else
			return StringCompare(data, other.data, size);
	}

	gcc_pure
	bool Equals(BasicStringView<T> other) const noexcept {
		return this->size == other.size &&
			StringIsEqual(data, other.data, this->size);
	}

	gcc_pure
	bool StartsWithIgnoreCase(BasicStringView<T> needle) const noexcept {
		return this->size >= needle.size &&
			StringIsEqualIgnoreCase(data, needle.data, needle.size);
	}

	gcc_pure
	bool EndsWithIgnoreCase(BasicStringView<T> needle) const noexcept {
		return this->size >= needle.size &&
			StringIsEqualIgnoreCase(data + this->size - needle.size,
						needle.data, needle.size);
	}

	gcc_pure
	bool EqualsIgnoreCase(BasicStringView<T> other) const noexcept {
		return this->size == other.size &&
			StringIsEqualIgnoreCase(data, other.data, this->size);
	}

	/**
	 * Skip all whitespace at the beginning.
	 */
	void StripLeft() noexcept;

	/**
	 * Skip all whitespace at the end.
	 */
	void StripRight() noexcept;

	void Strip() noexcept {
		StripLeft();
		StripRight();
	}

	bool SkipPrefix(BasicStringView<T> needle) noexcept {
		bool match = StartsWith(needle);
		if (match)
			skip_front(needle.size);
		return match;
	}

	bool RemoveSuffix(BasicStringView<T> needle) noexcept {
		bool match = EndsWith(needle);
		if (match)
			size -= needle.size;
		return match;
	}
};

struct StringView : BasicStringView<char> {
	using BasicStringView::BasicStringView;

	StringView() = default;
	constexpr StringView(BasicStringView<value_type> src) noexcept
		:BasicStringView(src) {}
};

#endif
