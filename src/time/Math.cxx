// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Math.hxx"
#include "Calendar.hxx"
#include "Convert.hxx"

#include <time.h>

std::chrono::system_clock::time_point
PrecedingMidnightLocal(std::chrono::system_clock::time_point t) noexcept
try {
	auto tm = LocalTime(t);
	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	return MakeTime(tm);
} catch (...) {
	/* best-effort fallback for this exotic error condition */
	return t;
 }

static void
IncrementMonth(struct tm &tm) noexcept
{
	++tm.tm_mon;

	if (tm.tm_mon >= 12) {
		/* roll over to next year */
		tm.tm_mon = 0;
		++tm.tm_year;
	}
}

void
EndOfMonth(struct tm &tm) noexcept
{
	tm.tm_sec = 0;
	tm.tm_min = 0;
	tm.tm_hour = 0;
	tm.tm_mday = 1;
	IncrementMonth(tm);
}

void
IncrementDay(struct tm &tm) noexcept
{
	const unsigned max_day = DaysInMonth(tm.tm_mon + 1, tm.tm_year + 1900);

	++tm.tm_mday;

	if ((unsigned)tm.tm_mday > max_day) {
		/* roll over to next month */
		tm.tm_mday = 1;
		IncrementMonth(tm);
	}

	++tm.tm_wday;
	if (tm.tm_wday >= 7)
		tm.tm_wday = 0;
}

void
DecrementDay(struct tm &tm) noexcept
{
	--tm.tm_mday;

	if (tm.tm_mday < 1) {
		/* roll over to previous month */

		--tm.tm_mon;
		if (tm.tm_mon < 0) {
			/* roll over to previous eyar */
			tm.tm_mon = 11;
			--tm.tm_year;
		}

		const unsigned max_day = DaysInMonth(tm.tm_mon + 1,
						     tm.tm_year + 1900);
		tm.tm_mday = max_day;
	}

	--tm.tm_wday;
	if (tm.tm_wday < 0)
		tm.tm_wday = 6;
}
