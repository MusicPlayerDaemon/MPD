/*
 * Copyright 2007-2019 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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
