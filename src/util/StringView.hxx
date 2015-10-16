/*
 * Copyright (C) 2013-2015 Max Kellermann <max@duempel.org>
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

#include <string.h>

struct StringView : ConstBuffer<char> {
	StringView() = default;

	constexpr StringView(pointer_type _data, size_type _size)
		:ConstBuffer<char>(_data, _size) {}

	constexpr StringView(pointer_type _begin, pointer_type _end)
		:ConstBuffer<char>(_begin, _end - _begin) {}

	StringView(pointer_type _data)
		:ConstBuffer<char>(_data,
				   _data != nullptr ? strlen(_data) : 0) {}

	StringView(std::nullptr_t n)
		:ConstBuffer<char>(n) {}

	static constexpr StringView Empty() {
		return StringView("", size_t(0));
	}

	void SetEmpty() {
		data = "";
		size = 0;
	}

	gcc_pure
	pointer_type Find(char ch) const {
		return (pointer_type)memchr(data, ch, size);
	}

	StringView &operator=(std::nullptr_t) {
		data = nullptr;
		size = 0;
		return *this;
	}

	StringView &operator=(pointer_type _data) {
		data = _data;
		size = _data != nullptr ? strlen(_data) : 0;
		return *this;
	}

	gcc_pure
	bool StartsWith(StringView needle) const {
		return size >= needle.size &&
			memcmp(data, needle.data, needle.size) == 0;
	}

	gcc_pure
	bool Equals(StringView other) const {
		return size == other.size &&
			memcmp(data, other.data, size) == 0;
	}

	template<size_t n>
	bool EqualsLiteral(const char (&other)[n]) const {
		return Equals({other, n - 1});
	}

	gcc_pure
	bool EqualsIgnoreCase(StringView other) const {
		return size == other.size &&
			strncasecmp(data, other.data, size) == 0;
	}

	template<size_t n>
	bool EqualsLiteralIgnoreCase(const char (&other)[n]) const {
		return EqualsIgnoreCase({other, n - 1});
	}

	/**
	 * Skip all whitespace at the beginning.
	 */
	void StripLeft();

	/**
	 * Skip all whitespace at the end.
	 */
	void StripRight();
};

#endif
