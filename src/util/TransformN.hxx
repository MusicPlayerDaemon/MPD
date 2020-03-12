/*
 * Copyright 2019 Max Kellermann <max.kellermann@gmail.com>
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
