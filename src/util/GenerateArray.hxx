/*
 * Copyright 2020 Max Kellermann <max.kellermann@gmail.com>
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

