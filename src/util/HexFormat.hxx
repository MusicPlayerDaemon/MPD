/*
 * Copyright 2007-2022 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "ConstBuffer.hxx"
#include "StringBuffer.hxx"

#include <cstddef>
#include <cstdint>

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
HexFormat(char *output, ConstBuffer<uint8_t> input) noexcept
{
	for (const auto &i : input)
		output = HexFormatUint8Fixed(output, i);

	return output;
}

/**
 * Like HexFormat(), but return a #StringBuffer with exactly the
 * required size.
 */
template<size_t size>
[[gnu::pure]]
constexpr auto
HexFormatBuffer(const uint8_t *src) noexcept
{
	StringBuffer<size * 2 + 1> dest;
	*HexFormat(dest.data(), {src, size}) = 0;
	return dest;
}
