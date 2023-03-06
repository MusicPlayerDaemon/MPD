// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <gcrypt.h>

#include <array>
#include <span>

namespace Gcrypt {

template<int algo, std::size_t size>
[[gnu::pure]]
auto
Hash(std::span<const std::byte> input) noexcept
{
	std::array<std::byte, size> result;
	gcry_md_hash_buffer(algo, &result.front(),
			    input.data(), input.size());
	return result;
}

} /* namespace Gcrypt */
