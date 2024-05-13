// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <concepts>

template<std::unsigned_integral T>
constexpr T
RoundUpToPowerOfTwo(T value) noexcept
{
	if (value <= 0)
		return 1;

	--value;

	for (unsigned bits = 1; bits < sizeof(T) * 8; bits <<= 1)
		value |= value >> bits;

	++value;

	return value;
}

static_assert(RoundUpToPowerOfTwo(0U) == 1U);
static_assert(RoundUpToPowerOfTwo(2U) == 2U);
static_assert(RoundUpToPowerOfTwo(3U) == 4U);
static_assert(RoundUpToPowerOfTwo(4U) == 4U);
static_assert(RoundUpToPowerOfTwo(5U) == 8U);
static_assert(RoundUpToPowerOfTwo(0x7fffU) == 0x8000U);
static_assert(RoundUpToPowerOfTwo(0x7ffffU) == 0x80000U);
static_assert(RoundUpToPowerOfTwo(0x1000000000000000ULL) == 0x1000000000000000ULL);
static_assert(RoundUpToPowerOfTwo(0x1fffffffffffffffULL) == 0x2000000000000000ULL);
static_assert(RoundUpToPowerOfTwo(0x7fffffffffffffffULL) == 0x8000000000000000ULL);
static_assert(RoundUpToPowerOfTwo(0x8000000000000000ULL) == 0x8000000000000000ULL);

template<std::unsigned_integral T>
constexpr T
RoundUpToPowerOfTwo(T value, T power_of_two) noexcept
{
	return ((value - 1) | (power_of_two - 1)) + 1;
}

template<std::unsigned_integral T>
constexpr T
RoundDownToPowerOfTwo(T value, T power_of_two) noexcept
{
	return value & ~(power_of_two - 1);
}
