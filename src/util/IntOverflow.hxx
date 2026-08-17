// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <concepts> // for std::integral

#ifndef __GNUC__
#include <limits>
#endif

/**
 * Portable wrapper for the GCC built-in __builtin_add_overflow().
 */
template<std::unsigned_integral T>
[[nodiscard]] [[gnu::always_inline]]
constexpr bool
AddOverflow(T a, T b, T &result) noexcept
{
#ifdef __GNUC__
	bool overflow = __builtin_add_overflow(a, b, &result);
#else
	result = a + b;
	return b > std::numeric_limits<T>::max() - a;
#endif

	// a portable version of __builtin_expect(overflow, false)
	if (overflow)
		[[unlikely]]
		return true;
	else
		[[likely]]
		return false;
}
