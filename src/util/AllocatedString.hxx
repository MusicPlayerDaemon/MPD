/*
 * Copyright 2015-2019 Max Kellermann <max.kellermann@gmail.com>
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

/**
 * A string pointer whose memory is managed by this class.
 *
 * Unlike std::string, this object can hold a "nullptr" special value.
 */
template<typename T=char>
class AllocatedString {
public:
	typedef typename StringPointer<T>::value_type value_type;
	typedef typename StringPointer<T>::reference reference;
	typedef typename StringPointer<T>::const_reference const_reference;
	typedef typename StringPointer<T>::pointer pointer;
	typedef typename StringPointer<T>::const_pointer const_pointer;
	typedef std::size_t size_type;

	static constexpr value_type SENTINEL = '\0';

private:
	pointer value;

	explicit AllocatedString(pointer _value)
		:value(_value) {}

public:
	AllocatedString(std::nullptr_t n):value(n) {}

	AllocatedString(AllocatedString &&src)
		:value(src.Steal()) {}

	~AllocatedString() {
		delete[] value;
	}

	static AllocatedString Donate(pointer value) {
		return AllocatedString(value);
	}

	static AllocatedString Null() {
		return nullptr;
	}

	static AllocatedString Empty() {
		auto p = new value_type[1];
		p[0] = SENTINEL;
		return Donate(p);
	}

	static AllocatedString Duplicate(const_pointer src);

	static AllocatedString Duplicate(const_pointer begin,
					 const_pointer end) {
		auto p = new value_type[end - begin + 1];
		*std::copy(begin, end, p) = SENTINEL;
		return Donate(p);
	}

	static AllocatedString Duplicate(const_pointer begin,
					 size_type length) {
		auto p = new value_type[length + 1];
		*std::copy_n(begin, length, p) = SENTINEL;
		return Donate(p);
	}

	AllocatedString &operator=(AllocatedString &&src) {
		std::swap(value, src.value);
		return *this;
	}

	constexpr bool operator==(std::nullptr_t) const {
		return value == nullptr;
	}

	constexpr bool operator!=(std::nullptr_t) const {
		return value != nullptr;
	}

	constexpr bool IsNull() const {
		return value == nullptr;
	}

	constexpr const_pointer c_str() const {
		return value;
	}

	bool empty() const {
		return *value == SENTINEL;
	}

	constexpr pointer data() const {
		return value;
	}

	reference operator[](size_type i) {
		return value[i];
	}

	const reference operator[](size_type i) const {
		return value[i];
	}

	pointer Steal() {
		return std::exchange(value, nullptr);
	}

	AllocatedString Clone() const {
		return Duplicate(c_str());
	}
};

#endif
