// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

static constexpr std::size_t INT_HASH_INIT = 17;

/**
 * A very simple/naive and fast hash function for integers of
 * arbitrary size.
 */
template<std::integral T>
[[nodiscard]] [[gnu::always_inline]] [[gnu::hot]]
constexpr std::size_t
IntHashUpdate(T src, std::size_t hash) noexcept
{
	return (hash * 19) + static_cast<std::size_t>(src);
}

template<std::integral T, std::size_t extent=std::dynamic_extent>
[[nodiscard]] [[gnu::hot]]
constexpr std::size_t
IntHash(std::span<const T, extent> src, std::size_t hash=INT_HASH_INIT) noexcept
{
	for (const T i : src)
		hash = IntHashUpdate(i, hash);
	return hash;
}

template<typename T, typename I>
requires(std::has_unique_object_representations_v<T> && sizeof(T) % sizeof(I) == 0 && alignof(T) % sizeof(I) == 0)
[[nodiscard]] [[gnu::hot]]
constexpr std::size_t
_IntHashT(const T &src, std::size_t hash=INT_HASH_INIT) noexcept
{
	constexpr std::size_t n = sizeof(src) / sizeof(I);
	const std::span<const I, n> span{reinterpret_cast<const I *>(&src), n};
	return IntHash(span, hash);
}

/**
 * Calculate the hash of an arbitrary (trivial) object, using the
 * largest integer according to the object's alignment at compile
 * time.
 */
template<typename T>
requires std::has_unique_object_representations_v<T>
[[nodiscard]] [[gnu::hot]]
constexpr std::size_t
IntHashT(const T &src, std::size_t hash=INT_HASH_INIT) noexcept
{
	if constexpr (sizeof(T) % 8 == 0)
		return _IntHashT<T, uint64_t>(src, hash);
	else if constexpr (sizeof(T) % 4 == 0)
		return _IntHashT<T, uint32_t>(src, hash);
	else if constexpr (sizeof(T) % 2 == 0)
		return _IntHashT<T, uint16_t>(src, hash);
	else
		return _IntHashT<T, uint8_t>(src, hash);
}
