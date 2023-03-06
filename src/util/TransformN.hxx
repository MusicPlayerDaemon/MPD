// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef TRANSFORM_N_HXX
#define TRANSFORM_N_HXX

#include <cstddef>

/**
 * The std::transform_n() function that is missing in the C++ standard
 * library.
 */
template<class InputIt, class OutputIt, class UnaryOperation>
OutputIt
transform_n(InputIt input, size_t n, OutputIt output,
	    UnaryOperation unary_op)
{
	while (n-- > 0)
		*output++ = unary_op(*input++);
	return output;
}

/**
 * Optimized overload for the above transform_n() implementation for
 * the special case that both input and output are regular pointers.
 * Turns out that most compilers generate better code this way.
 */
template<class InputType, class OutputType, class UnaryOperation>
OutputType *
transform_n(const InputType *input, size_t n, OutputType *output,
	    UnaryOperation unary_op)
{
	for (size_t i = 0; i < n; ++i)
		output[i] = unary_op(input[i]);
	return output + n;
}

#endif
