// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#ifndef TIME_ISO8601_HXX
#define TIME_ISO8601_HXX

#include <chrono>
#include <cstddef>
#include <utility>

struct tm;
template<size_t CAPACITY> class StringBuffer;

[[gnu::pure]]
StringBuffer<64>
FormatISO8601(const struct tm &tm) noexcept;

[[gnu::pure]]
StringBuffer<64>
FormatISO8601(std::chrono::system_clock::time_point tp);

/**
 * Parse a time stamp in ISO8601 format.
 *
 * Throws on error.
 *
 * @return a pair consisting of the time point and the specified
 * precision; e.g. for a date, the second value is "one day"
 */
std::pair<std::chrono::system_clock::time_point,
	  std::chrono::system_clock::duration>
ParseISO8601(const char *s);

#endif
