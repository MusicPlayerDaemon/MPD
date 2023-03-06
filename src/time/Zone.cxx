// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Zone.hxx"

#ifdef _WIN32
#include <profileapi.h>
#include <sysinfoapi.h>
#include <timezoneapi.h>
#else
#include <time.h>
#endif

int
GetTimeZoneOffset() noexcept
{
#ifdef _WIN32
	TIME_ZONE_INFORMATION TimeZoneInformation;
	DWORD tzi = GetTimeZoneInformation(&TimeZoneInformation);

	int offset = -TimeZoneInformation.Bias * 60;
	if (tzi == TIME_ZONE_ID_STANDARD)
		offset -= TimeZoneInformation.StandardBias * 60;

	if (tzi == TIME_ZONE_ID_DAYLIGHT)
		offset -= TimeZoneInformation.DaylightBias * 60;

	return offset;
#else
	time_t t = 1234567890;
	struct tm tm;
	tm.tm_isdst = 0;
	struct tm *p = &tm;
	gmtime_r(&t, p);
	return t - mktime(p);
#endif
}
