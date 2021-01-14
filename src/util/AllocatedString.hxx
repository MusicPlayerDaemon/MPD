/*
 * Copyright 2015-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef ALLOCATED_STRING_HXX
#define ALLOCATED_STRING_HXX

#include "StringPointer.hxx"

#include <algorithm>
#include <cstddef>
#include <string_view>

/**
 * A string pointer whose memory is managed by this class.
 *
 * Unlike std::string, this object can hold a "nullptr" special value.
 */
template<typename T>
class BasicAllocatedString {
public:
	using value_type = typename StringPointer<T>::value_type;
	using reference = typename StringPointer<T>::reference;
	using const_reference = typename StringPointer<T>::const_reference;
	using pointer = typename StringPointer<T>::pointer;
	using const_pointer = typename StringPointer<T>::const_pointer;
	using string_view = std::basic_string_view<T>;
	using size_type = std::size_t;

	static constexpr value_type SENTINEL = '\0';

private:
	pointer value;

	explicit BasicAllocatedString(pointer _value) noexcept
		:value(_value) {}

public:
	BasicAllocatedString(std::nullptr_t n) noexcept
		:value(n) {}

	BasicAllocatedString(BasicAllocatedString &&src) noexcept
		:value(src.Steal()) {}

	~BasicAllocatedString() noexcept {
		delete[] value;
	}

	static BasicAllocatedString Donate(pointer value) noexcept {
		return BasicAllocatedString(value);
	}

	static BasicAllocatedString Null() noexcept {
		return nullptr;
	}

	static BasicAllocatedString Empty() {
		auto p = new value_type[1];
		p[0] = SENTINEL;
		return Donate(p);
	}

	static BasicAllocatedString Duplicate(string_view src) {
		auto p = new value_type[src.size() + 1];
		*std::copy_n(src.data(), src.size(), p) = SENTINEL;
		return Donate(p);
	}

	BasicAllocatedString &operator=(BasicAllocatedString &&src) noexcept {
		std::swap(value, src.value);
		return *this;
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return value == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return value != nullptr;
	}

	constexpr bool IsNull() const noexcept {
		return value == nullptr;
	}

	operator string_view() const noexcept {
		return value;
	}

	constexpr const_pointer c_str() const noexcept {
		return value;
	}

	bool empty() const noexcept {
		return *value == SENTINEL;
	}

	constexpr pointer data() const noexcept {
		return value;
	}

	reference operator[](size_type i) noexcept {
		return value[i];
	}

	const reference operator[](size_type i) const noexcept {
		return value[i];
	}

	pointer Steal() noexcept {
		return std::exchange(value, nullptr);
	}

	BasicAllocatedString Clone() const {
		return Duplicate(*this);
	}
};

class AllocatedString : public BasicAllocatedString<char> {
public:
	using BasicAllocatedString::BasicAllocatedString;

	AllocatedString(BasicAllocatedString<value_type> &&src) noexcept
		:BasicAllocatedString(std::move(src)) {}
};

#endif
