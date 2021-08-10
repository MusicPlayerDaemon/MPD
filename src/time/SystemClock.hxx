/*
 * Copyright 2021 Max Kellermann <max.kellermann@gmail.com>
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

#pragma once

#include <chrono>

/**
 * Calculate a std::chrono::system_clock::time_point from a duration
 * relative to the UNIX epoch (1970-01-01T00:00Z).
 */
template<class Rep, class Period>
[[gnu::const]]
std::chrono::system_clock::time_point
TimePointAfterUnixEpoch(const std::chrono::duration<Rep,Period> &d) noexcept
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
DurationSinceUnixEpoch(const std::chrono::system_clock::time_point &tp) noexcept
{
	/* this is guaranteed to be 0 in C++20 */
	const auto unix_epoch = std::chrono::system_clock::from_time_t(0);

	return tp - unix_epoch;
}
