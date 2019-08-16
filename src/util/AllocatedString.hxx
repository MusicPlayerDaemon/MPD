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

/**
 * A string pointer whose memory is managed by this class.
 *
 * Unlike std::string, this object can hold a "nullptr" special value.
 */
template<typename T=char>
class AllocatedString {
public:
	typedef typename StringPointer<T>::value_type value_type;
	typedef typename StringPointer<T>::reference_type reference_type;
	typedef typename StringPointer<T>::const_reference_type const_reference_type;
	typedef typename StringPointer<T>::pointer_type pointer_type;
	typedef typename StringPointer<T>::const_pointer_type const_pointer_type;
	typedef size_t size_type;

	static constexpr value_type SENTINEL = '\0';

private:
	pointer_type value;

	explicit AllocatedString(pointer_type _value)
		:value(_value) {}

public:
	AllocatedString(std::nullptr_t n):value(n) {}

	AllocatedString(AllocatedString &&src)
		:value(src.Steal()) {}

	~AllocatedString() {
		delete[] value;
	}

	static AllocatedString Donate(pointer_type value) {
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

	static AllocatedString Duplicate(const_pointer_type src);

	static AllocatedString Duplicate(const_pointer_type begin,
					 const_pointer_type end) {
		auto p = new value_type[end - begin + 1];
		*std::copy(begin, end, p) = SENTINEL;
		return Donate(p);
	}

	static AllocatedString Duplicate(const_pointer_type begin,
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

	constexpr const_pointer_type c_str() const {
		return value;
	}

	bool empty() const {
		return *value == SENTINEL;
	}

	constexpr pointer_type data() const {
		return value;
	}

	reference_type operator[](size_type i) {
		return value[i];
	}

	const reference_type operator[](size_type i) const {
		return value[i];
	}

	pointer_type Steal() {
		pointer_type result = value;
		value = nullptr;
		return result;
	}

	AllocatedString Clone() const {
		return Duplicate(c_str());
	}
};

#endif
