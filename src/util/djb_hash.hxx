// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

/*
 * Implementation of D. J. Bernstein's cdb hash function.
 * http://cr.yp.to/cdb/cdb.txt
 */

#pragma once

#include <cstddef>
#include <span>

static constexpr std::size_t DJB_HASH_INIT = 5381;

[[nodiscard]] [[gnu::always_inline]] [[gnu::hot]]
constexpr std::size_t
djb_hash_update(std::size_t hash, std::byte b) noexcept
{
	return (hash * 33) ^ static_cast<std::size_t>(b);
}

[[nodiscard]] [[gnu::hot]]
constexpr std::size_t
djb_hash(std::span<const std::byte> src,
	 std::size_t init=DJB_HASH_INIT) noexcept
{
	std::size_t hash = init;

	for (const auto i : src)
		hash = djb_hash_update(hash, i);

	return hash;
}

[[nodiscard]] [[gnu::hot]]
constexpr std::size_t
djb_hash_string(const char *p,
		std::size_t init=DJB_HASH_INIT) noexcept
{
	std::size_t hash = init;

	while (*p != 0)
		hash = djb_hash_update(hash, static_cast<std::byte>(*p++));

	return hash;
}
