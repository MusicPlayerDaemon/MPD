// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef CHRONO_UTIL_HXX
#define CHRONO_UTIL_HXX

#include <chrono>

template<typename Clock, typename Duration>
constexpr bool
IsNegative(const std::chrono::time_point<Clock, Duration> p)
{
	return p < std::chrono::time_point<Clock, Duration>();
}

#endif
