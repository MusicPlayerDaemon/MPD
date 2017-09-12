/*
 * Copyright (C) 2013-2017 Max Kellermann <max.kellermann@gmail.com>
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

struct StringView : ConstBuffer<char> {
	StringView() = default;

	constexpr StringView(pointer_type _data, size_type _size) noexcept
		:ConstBuffer(_data, _size) {}

	constexpr StringView(pointer_type _begin, pointer_type _end) noexcept
		:ConstBuffer(_begin, _end - _begin) {}

	StringView(pointer_type _data) noexcept
		:ConstBuffer(_data,
			     _data != nullptr ? StringLength(_data) : 0) {}

	constexpr StringView(std::nullptr_t n) noexcept
		:ConstBuffer(n) {}

	gcc_pure
	pointer_type Find(value_type ch) const noexcept {
		return StringFind(data, ch, size);
	}

	gcc_pure
	bool StartsWith(StringView needle) const noexcept {
		return size >= needle.size &&
			StringIsEqual(data, needle.data, needle.size);
	}

	gcc_pure
	bool EndsWith(StringView needle) const noexcept {
		return size >= needle.size &&
			StringIsEqual(data + size - needle.size,
				      needle.data, needle.size);
	}

	gcc_pure
	bool Equals(StringView other) const noexcept {
		return size == other.size &&
			StringIsEqual(data, other.data, size);
	}

	gcc_pure
	bool EqualsIgnoreCase(StringView other) const noexcept {
		return size == other.size &&
			StringIsEqualIgnoreCase(data, other.data, size);
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
};

#endif
