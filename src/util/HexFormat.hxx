// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <array>
#include <cstdint>
#include <span>

constexpr char hex_digits[] = "0123456789abcdef";

[[gnu::always_inline]]
static constexpr char *
HexFormatUint8Fixed(char dest[2], uint8_t number) noexcept
{
	dest[0] = hex_digits[(number >> 4) & 0xf];
	dest[1] = hex_digits[number & 0xf];
	return dest + 2;
}

[[gnu::always_inline]]
static constexpr char *
HexFormatUint16Fixed(char dest[4], uint16_t number) noexcept
{
	dest[0] = hex_digits[(number >> 12) & 0xf];
	dest[1] = hex_digits[(number >> 8) & 0xf];
	dest[2] = hex_digits[(number >> 4) & 0xf];
	dest[3] = hex_digits[number & 0xf];
	return dest + 4;
}

[[gnu::always_inline]]
static constexpr char *
HexFormatUint32Fixed(char dest[8], uint32_t number) noexcept
{
	dest[0] = hex_digits[(number >> 28) & 0xf];
	dest[1] = hex_digits[(number >> 24) & 0xf];
	dest[2] = hex_digits[(number >> 20) & 0xf];
	dest[3] = hex_digits[(number >> 16) & 0xf];
	dest[4] = hex_digits[(number >> 12) & 0xf];
	dest[5] = hex_digits[(number >> 8) & 0xf];
	dest[6] = hex_digits[(number >> 4) & 0xf];
	dest[7] = hex_digits[number & 0xf];
	return dest + 8;
}

[[gnu::always_inline]]
static constexpr char *
HexFormatUint64Fixed(char dest[16], uint64_t number) noexcept
{
	dest = HexFormatUint32Fixed(dest, number >> 32);
	dest = HexFormatUint32Fixed(dest, number);
	return dest;
}

/**
 * Format the given input buffer of bytes to hex.  The caller ensures
 * that the output buffer is at least twice as large as the input.
 * Does not null-terminate the output buffer.
 *
 * @return a pointer to one after the last written character
 */
constexpr char *
HexFormat(char *output, std::span<const std::byte> input) noexcept
{
	for (const auto &i : input)
		output = HexFormatUint8Fixed(output, (uint8_t)i);

	return output;
}

/**
 * Return a std::array<char> (not null-terminated) containing a hex
 * dump of the given fixed-size input.
 */
template<std::size_t size>
constexpr auto
HexFormat(std::span<const std::byte, size> input) noexcept
{
	std::array<char, size * 2> output;
	HexFormat(output.data(), input);
	return output;
}
