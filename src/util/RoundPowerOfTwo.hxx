// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <concepts>

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
