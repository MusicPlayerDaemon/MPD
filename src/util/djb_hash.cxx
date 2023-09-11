// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "djb_hash.hxx"

#include <cassert>

[[gnu::always_inline]] [[gnu::hot]]
static constexpr std::size_t
djb_hash_update(std::size_t hash, std::byte b) noexcept
{
	return (hash * 33) ^ static_cast<std::size_t>(b);
}

[[gnu::hot]]
std::size_t
djb_hash(std::span<const std::byte> src, std::size_t init) noexcept
{
	std::size_t hash = init;

	for (const auto i : src)
		hash = djb_hash_update(hash, i);

	return hash;
}

[[gnu::hot]]
std::size_t
djb_hash_string(const char *p, std::size_t init) noexcept
{
	assert(p != nullptr);

	std::size_t hash = init;

	while (*p != 0)
		hash = djb_hash_update(hash, static_cast<std::byte>(*p++));

	return hash;
}
