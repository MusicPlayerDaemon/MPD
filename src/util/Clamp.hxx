// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CLAMP_HPP
#define CLAMP_HPP

#include "Compiler.h"

/**
 * Clamps the specified value in a range.  Returns #min or #max if the
 * value is outside.
 */
template<typename T>
constexpr const T &
Clamp(const T &value, const T &min, const T &max)
{
	return gcc_unlikely(value < min)
		? min
		: (gcc_unlikely(value > max)
		   ? max : value);
}

#endif
