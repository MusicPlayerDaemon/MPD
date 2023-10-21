// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "CopyConst.hxx"

#include <cassert>
#include <cstddef>
#include <span>
#include <string_view>

/**
 * Cast a std::span<std::byte> to a std::span<T>, rounding down to the
 * next multiple of T's size.
 */
template<typename T>
constexpr std::span<T>
FromBytesFloor(std::span<CopyConst<std::byte, T>> other) noexcept
{
	static_assert(sizeof(T) > 0, "Empty base type");

	/* TODO: the "void *" cast suppresses alignment
	   warnings, but should we really suppress them? */

	return {
		reinterpret_cast<T *>(reinterpret_cast<CopyConst<void, T> *>(other.data())),
		other.size() / sizeof(T),
	};
}

/**
 * Like FromBytesFloor(), but assert that rounding is not necessary.
 */
template<typename T>
constexpr std::span<T>
FromBytesStrict(std::span<CopyConst<std::byte, T>> other) noexcept
{
	assert(other.size() % sizeof(T) == 0);

	return FromBytesFloor<T>(other);
}

constexpr std::span<const char>
ToSpan(std::string_view sv) noexcept
{
#if defined(__clang__) && __clang_major__ < 15
	/* workaround for old clang/libc++ versions which can't cast
	   std::string_view to std::span */
	return {sv.data(), sv.size()};
#else
	return std::span{sv};
#endif
}

inline std::span<const std::byte>
AsBytes(std::string_view sv) noexcept
{
	return std::as_bytes(ToSpan(sv));
}

/**
 * Cast a reference to a fixed-size std::span<const std::byte>.
 */
template<typename T>
constexpr auto
ReferenceAsBytes(const T &value) noexcept
{
	return std::as_bytes(std::span<const T, 1>{&value, 1});
}

constexpr std::string_view
ToStringView(std::span<const char> s) noexcept
{
	return {s.data(), s.size()};
}

constexpr std::string_view
ToStringView(std::span<const std::byte> s) noexcept
{
	return ToStringView(FromBytesStrict<const char>(s));
}

template<typename T>
constexpr std::basic_string_view<T>
ToStringView(std::span<const T> s) noexcept
{
	return {s.data(), s.size()};
}

template<typename T>
constexpr std::basic_string_view<T>
ToStringView(std::span<T> s) noexcept
{
	return {s.data(), s.size()};
}
