// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef GENERATE_ARRAY_HXX
#define GENERATE_ARRAY_HXX

#include <array>
#include <utility>

template<std::size_t N, typename F, std::size_t... I>
constexpr auto
_GenerateArray(F &&f, std::index_sequence<I...>) noexcept
{
	using T = decltype(f(0));

	/* double curly braces for compatibility with older compilers
	   which are not 100% C++17 compliant (e.g. Apple xcode
	   9.4) */
	return std::array<T, N>{{f(I)...}};
}

/**
 * Generate a `constexpr` std::array at compile time by calling the
 * given function for each index.
 *
 * @param N the number of elements in the array
 * @param F the function (called N times with the index as only parameter)
 */
template<std::size_t N, typename F>
constexpr auto
GenerateArray(F &&f) noexcept
{
	return _GenerateArray<N>(std::forward<F>(f),
				 std::make_index_sequence<N>());
}

#endif

