// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <chrono>

/**
 * Calculate a std::chrono::system_clock::time_point from a duration
 * relative to the UNIX epoch (1970-01-01T00:00Z).
 */
template<class Rep, class Period>
[[gnu::const]]
std::chrono::system_clock::time_point
TimePointAfterUnixEpoch(const std::chrono::duration<Rep,Period> d) noexcept
{
	/* this is guaranteed to be 0 in C++20 */
	const auto unix_epoch = std::chrono::system_clock::from_time_t(0);

	return unix_epoch +
		std::chrono::duration_cast<std::chrono::system_clock::duration>(d);
}

/**
 * Calculate the duration that has passed since the UNIX epoch
 * (1970-01-01T00:00Z).
 */
[[gnu::const]]
inline std::chrono::system_clock::duration
DurationSinceUnixEpoch(const std::chrono::system_clock::time_point tp) noexcept
{
	/* this is guaranteed to be 0 in C++20 */
	const auto unix_epoch = std::chrono::system_clock::from_time_t(0);

	return tp - unix_epoch;
}
