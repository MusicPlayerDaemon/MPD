/*
 * Copyright 2015-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef TEMPLATE_STRING_HXX
#define TEMPLATE_STRING_HXX

#include "StringView.hxx"

#include <array> // for std::size()
#include <cstddef>
#include <string_view>

namespace TemplateString {

template<std::size_t _size>
struct Buffer {
	static constexpr std::size_t size = _size;
	char value[size + 1];

	constexpr operator const char *() const noexcept {
		return value;
	}

	constexpr operator std::string_view() const noexcept {
		return {value, size};
	}

	constexpr operator StringView() const noexcept {
		return {value, size};
	}
};

/**
 * An empty string.
 */
constexpr auto
Empty() noexcept
{
	return Buffer<0>{};
}

/**
 * A string consisting of a single character.
 */
constexpr auto
FromChar(char ch) noexcept
{
	Buffer<1> result{};
	result.value[0] = ch;
	return result;
}

namespace detail {

constexpr auto
size(const char &) noexcept
{
	return 1;
}

constexpr const char *
data(const char &ch) noexcept
{
	return &ch;
}

template<std::size_t s>
constexpr auto
size(const Buffer<s> &b) noexcept
{
	return b.size;
}

template<std::size_t size>
constexpr const char *
data(const Buffer<size> &b) noexcept
{
	return b.value;
}

constexpr char *
copy_n(const char *src, std::size_t n, char *dest) noexcept
{
	for (std::size_t i = 0; i < n; ++i)
		dest[i] = src[i];
	return dest + n;
}

}

/**
 * A string consisting of a single character.
 */
template<std::size_t size>
constexpr auto
FromLiteral(const char (&src)[size]) noexcept
{
	Buffer<size - 1> result{};
	detail::copy_n(src, result.size, result.value);
	return result;
}

template<typename... Args>
constexpr auto
Concat(Args... args) noexcept
{
	using std::size;
	using std::data;
	using detail::size;
	using detail::data;
	using detail::copy_n;

	constexpr std::size_t total_size = (std::size_t(0) + ... + size(args));
	Buffer<total_size> result{};

	char *p = result.value;
	((p = copy_n(data(args), size(args), p)), ...);

	return result;
}

} // namespace TemplateString

#endif
