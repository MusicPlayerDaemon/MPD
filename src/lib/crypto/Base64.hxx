// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

template<typename T> class AllocatedArray;

constexpr size_t
CalculateBase64OutputSize(size_t in_size) noexcept
{
	return in_size * 3 / 4;
}

/**
 * Throws on error.
 */
size_t
DecodeBase64(std::span<std::byte> out, std::string_view in);

/**
 * Throws on error.
 */
size_t
DecodeBase64(std::span<std::byte> out, const char *in);

/**
 * Throws on error.
 *
 * @return the decoded string
 */
AllocatedArray<std::byte>
DecodeBase64(std::string_view src);
