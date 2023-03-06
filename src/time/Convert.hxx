// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef TIME_CONVERT_HXX
#define TIME_CONVERT_HXX

#include <chrono>

/**
 * Convert a UTC-based time point to a UTC-based "struct tm".
 *
 * Throws on error.
 */
struct tm
GmTime(std::chrono::system_clock::time_point tp);

/**
 * Convert a UTC-based time point to a local "struct tm".
 *
 * Throws on error.
 */
struct tm
LocalTime(std::chrono::system_clock::time_point tp);

/**
 * Convert a UTC-based "struct tm" to a UTC-based time point.
 */
[[gnu::pure]]
std::chrono::system_clock::time_point
TimeGm(struct tm &tm) noexcept;

/**
 * Convert a local "struct tm" to a UTC-based time point.
 */
[[gnu::pure]]
std::chrono::system_clock::time_point
MakeTime(struct tm &tm) noexcept;

[[gnu::pure]]
std::chrono::steady_clock::duration
ToSteadyClockDuration(const struct timeval &tv) noexcept;

#endif
