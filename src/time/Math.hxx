/*
 * Copyright 2007-2020 CM4all GmbH
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

#pragma once

#include <chrono>

struct tm;

/**
 * Calculates the preceding midnight time point in the current time
 * zone.
 */
[[gnu::const]]
std::chrono::system_clock::time_point
PrecedingMidnightLocal(std::chrono::system_clock::time_point t) noexcept;

/**
 * Calculate the end of the current month (i.e. midnight on the 1st of
 * the following month).  Does NOT keeps the tm_wday and tm_yday
 * attributes updated, and ignores day light saving transitions.
 */
void
EndOfMonth(struct tm &tm) noexcept;

/**
 * Calculate the next day, keeping month/year wraparounds and leap
 * days in mind.  Keeps the tm_wday attribute updated, but not other
 * derived attributes such as tm_yday, and ignores day light saving
 * transitions.
 */
void
IncrementDay(struct tm &tm) noexcept;

/**
 * Calculate the previous day, keeping month/year wraparounds and leap
 * days in mind.  Keeps the tm_wday attribute updated, but not other
 * derived attributes such as tm_yday, and ignores day light saving
 * transitions.
 */
void
DecrementDay(struct tm &tm) noexcept;
