/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include <cassert>
#include <cstddef>
#include <span>
#include <string_view>

template<typename From, typename To>
using CopyConst = std::conditional_t<std::is_const_v<From>, const To, To>;

/**
 * Cast a std::span<std::byte> to a std::span<T>, rounding down to the
 * next multiple of T's size.
 */
template<typename T>
constexpr std::span<T>
FromBytesFloor(std::span<CopyConst<T, std::byte>> other) noexcept
{
	static_assert(sizeof(T) > 0, "Empty base type");

	/* TODO: the "void *" cast suppresses alignment
	   warnings, but should we really suppress them? */

	return {
		reinterpret_cast<T *>(reinterpret_cast<CopyConst<T, void> *>(other.data())),
		other.size() / sizeof(T),
	};
}

/**
 * Like FromBytesFloor(), but assert that rounding is not necessary.
 */
template<typename T>
constexpr std::span<T>
FromBytesStrict(std::span<CopyConst<T, std::byte>> other) noexcept
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
