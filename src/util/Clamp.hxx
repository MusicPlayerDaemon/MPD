// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CLAMP_HPP
#define CLAMP_HPP

/**
 * Clamps the specified value in a range.  Returns #min or #max if the
 * value is outside.
 */
template<typename T>
constexpr const T &
Clamp(const T &value, const T &min, const T &max) noexcept
{
	if (value < min) [[unlikely]]
		return min;

	if (value > max) [[unlikely]]
		return max;

	return value;
}

#endif
