/*
 * Copyright 2012-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef DEREFERENCE_ITERATOR_HXX
#define DEREFERENCE_ITERATOR_HXX

#include <iterator>
#include <type_traits>

/**
 * An iterator wrapper that dereferences the values returned by the
 * original iterator.
 */
template<typename IT,
	 typename VT=typename std::remove_pointer<typename IT::value_type>::type>
class DereferenceIterator {
	using Traits = std::iterator_traits<IT>;

	IT original;

public:
	using iterator_category = typename Traits::iterator_category;
	using difference_type = typename Traits::difference_type;
	using value_type = VT;
	using pointer = VT *;
	using reference = VT &;

	DereferenceIterator() = default;

	constexpr DereferenceIterator(const IT _original) noexcept
		:original(_original) {}

	reference operator*() const noexcept {
		return static_cast<reference>(**original);
	}

	pointer operator->() const noexcept {
		return static_cast<pointer>(&**original);
	}

	auto &operator++() noexcept {
		++original;
		return *this;
	}

	auto operator++(int) noexcept {
		auto old = *this;
		original++;
		return old;
	}

	auto &operator+=(difference_type n) noexcept {
		original += n;
		return *this;
	}

	auto &operator+(difference_type n) noexcept {
		return original + n;
	}

	auto &operator--() noexcept {
		original = --original;
		return *this;
	}

	auto operator--(int) noexcept {
		auto old = *this;
		original--;
		return old;
	}

	auto &operator-=(difference_type n) noexcept {
		original -= n;
		return *this;
	}

	auto &operator-(difference_type n) noexcept {
		return original - n;
	}

	bool operator==(const DereferenceIterator<IT,VT> &other) const noexcept {
		return original == other.original;
	}

	bool operator!=(const DereferenceIterator<IT,VT> &other) const noexcept {
		return original != other.original;
	}
};

#endif
