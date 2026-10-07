// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <concepts> // for std::integral

#ifndef __GNUC__
#include <limits>
#include <type_traits> // for std::common_type
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
	bool overflow = b > std::numeric_limits<T>::max() - a;
#endif

	// a portable version of __builtin_expect(overflow, false)
	if (overflow)
		[[unlikely]]
		return true;
	else
		[[likely]]
		return false;
}

/**
 * Portable wrapper for the GCC built-in __builtin_mul_overflow().
 */
template<std::unsigned_integral T>
[[nodiscard]] [[gnu::always_inline]]
constexpr bool
MultiplyOverflow(T a, T b, T &result) noexcept
{
#ifdef __GNUC__
	bool overflow = __builtin_mul_overflow(a, b, &result);
#else
	/* multiply in at least "unsigned" to avoid promotion to
           (signed) "int", which may overflow (UB) */
        using C = std::common_type_t<T, unsigned>;
        result = static_cast<T>(static_cast<C>(a) * static_cast<C>(b));

	bool overflow = b != 0 && a > std::numeric_limits<T>::max() / b;
#endif

	// a portable version of __builtin_expect(overflow, false)
	if (overflow)
		[[unlikely]]
		return true;
	else
		[[likely]]
		return false;
}
