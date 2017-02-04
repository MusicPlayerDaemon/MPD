/*
 * Copyright (C) 2015 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_POINTER_HXX
#define STRING_POINTER_HXX

/**
 * Simple OO wrapper for a const string pointer.
 */
template<typename T=char>
class StringPointer {
public:
	typedef T value_type;
	typedef T &reference_type;
	typedef const T &const_reference_type;
	typedef T *pointer_type;
	typedef const T *const_pointer_type;

	static constexpr value_type SENTINEL = '\0';

private:
	const_pointer_type value;

public:
	StringPointer() = default;
	constexpr StringPointer(const_pointer_type _value)
		:value(_value) {}

	/**
	 * Check if this is a "nulled" instance.  A "nulled" instance
	 * must not be used.
	 */
	constexpr bool IsNull() const {
		return value == nullptr;
	}

	constexpr const_pointer_type c_str() const {
		return value;
	}

	bool empty() const {
		return *value == SENTINEL;
	}
};

#endif
