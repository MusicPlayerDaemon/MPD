/*
 * Copyright 2007-2022 CM4all GmbH
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
