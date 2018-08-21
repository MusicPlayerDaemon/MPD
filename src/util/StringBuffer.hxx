/*
 * Copyright 2010-2018 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef STRING_BUFFER_HXX
#define STRING_BUFFER_HXX

#include <array>

/**
 * A statically allocated string buffer.
 */
template<typename T, size_t CAPACITY>
class BasicStringBuffer {
public:
	typedef T value_type;
	typedef T &reference;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef size_t size_type;

	static constexpr value_type SENTINEL = '\0';

protected:
	typedef std::array<value_type, CAPACITY> Array;
	Array the_data;

public:
	typedef typename Array::const_iterator const_iterator;

	constexpr size_type capacity() const noexcept {
		return CAPACITY;
	}

	constexpr bool empty() const noexcept {
		return front() == SENTINEL;
	}

	void clear() noexcept {
		the_data[0] = SENTINEL;
	}

	constexpr const_pointer c_str() const noexcept {
		return &the_data.front();
	}

	pointer data() noexcept {
		return &the_data.front();
	}

	constexpr value_type front() const noexcept {
		return the_data.front();
	}

	/**
	 * Returns one character.  No bounds checking.
	 */
	value_type operator[](size_type i) const noexcept {
		return the_data[i];
	}

	/**
	 * Returns one writable character.  No bounds checking.
	 */
	reference operator[](size_type i) noexcept {
		return the_data[i];
	}

	constexpr const_iterator begin() const noexcept {
		return the_data.begin();
	}

	constexpr const_iterator end() const noexcept {
		return the_data.end();
	}

	constexpr operator const_pointer() const noexcept {
		return c_str();
	}
};

template<size_t CAPACITY>
class StringBuffer : public BasicStringBuffer<char, CAPACITY> {};

#endif
