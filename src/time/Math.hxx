// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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
