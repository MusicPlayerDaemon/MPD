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
#include <utility>

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
	pointer value = nullptr;

	explicit BasicAllocatedString(pointer _value) noexcept
		:value(_value) {}

public:
	BasicAllocatedString() noexcept = default;
	BasicAllocatedString(std::nullptr_t n) noexcept
		:value(n) {}

	explicit BasicAllocatedString(string_view src)
		:value(Duplicate(src)) {}

	explicit BasicAllocatedString(const_pointer src)
		:value(Duplicate(src)) {}

	/**
	 * Concatenate several strings.
	 */
	BasicAllocatedString(std::initializer_list<string_view> src)
		:value(new value_type[TotalSize(src) + 1])
	{
		auto *p = value;
		for (const auto i : src)
			p = std::copy(i.begin(), i.end(), p);
		*p = SENTINEL;
	}

	BasicAllocatedString(const BasicAllocatedString &src) noexcept
		:BasicAllocatedString(Duplicate(src.value)) {}

	BasicAllocatedString(BasicAllocatedString &&src) noexcept
		:value(src.Steal()) {}

	~BasicAllocatedString() noexcept {
		delete[] value;
	}

	static BasicAllocatedString Donate(pointer value) noexcept {
		return BasicAllocatedString(value);
	}

	static BasicAllocatedString Empty() {
		auto p = new value_type[1];
		p[0] = SENTINEL;
		return Donate(p);
	}

	BasicAllocatedString &operator=(BasicAllocatedString &&src) noexcept {
		std::swap(value, src.value);
		return *this;
	}

	BasicAllocatedString &operator=(string_view src) noexcept {
		delete[] std::exchange(value, nullptr);
		value = Duplicate(src);
		return *this;
	}

	BasicAllocatedString &operator=(const_pointer src) noexcept {
		delete[] std::exchange(value, nullptr);
		value = src != nullptr ? Duplicate(src) : nullptr;
		return *this;
	}

	constexpr bool operator==(std::nullptr_t) const noexcept {
		return value == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const noexcept {
		return value != nullptr;
	}

	operator string_view() const noexcept {
		return value != nullptr
			? string_view(value)
			: string_view();
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

private:
	static pointer Duplicate(string_view src) {
		auto p = new value_type[src.size() + 1];
		*std::copy_n(src.data(), src.size(), p) = SENTINEL;
		return p;
	}

	static pointer Duplicate(const_pointer src) {
		return src != nullptr
			? Duplicate(string_view(src))
			: nullptr;
	}

	static constexpr std::size_t TotalSize(std::initializer_list<string_view> src) noexcept {
		std::size_t size = 0;
		for (std::string_view i : src)
			size += i.size();
		return size;
	}
};

class AllocatedString : public BasicAllocatedString<char> {
public:
	using BasicAllocatedString::BasicAllocatedString;

	AllocatedString() noexcept = default;
	AllocatedString(BasicAllocatedString<value_type> &&src) noexcept
		:BasicAllocatedString(std::move(src)) {}

	using BasicAllocatedString::operator=;
};

#endif
