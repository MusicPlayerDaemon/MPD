// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef TEMPLATE_STRING_HXX
#define TEMPLATE_STRING_HXX

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
