/*
 * Copyright (C) 2014-2017 Max Kellermann <max.kellermann@gmail.com>
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

#include "TimeParser.hxx"

#include <stdexcept>

#include <assert.h>
#include <time.h>

#if !defined(__GLIBC__) && !defined(WIN32)

/**
 * Determine the time zone offset in a portable way.
 */
gcc_const
static time_t
GetTimeZoneOffset()
{
	time_t t = 1234567890;
	struct tm tm;
	tm.tm_isdst = 0;
	gmtime_r(&t, &tm);
	return t - mktime(&tm);
}

#endif

std::chrono::system_clock::time_point
ParseTimePoint(const char *s, const char *format)
{
	assert(s != nullptr);
	assert(format != nullptr);

#ifdef WIN32
	/* TODO: emulate strptime()? */
	throw std::runtime_error("Time parsing not implemented on Windows");
#else
	struct tm tm;
	const char *end = strptime(s, format, &tm);
	if (end == nullptr || *end != 0)
		throw std::runtime_error("Failed to parse time stamp");

#ifdef __GLIBC__
	/* timegm() is a GNU extension */
	const auto t = timegm(&tm);
#else
	tm.tm_isdst = 0;
	const auto t = mktime(&tm) + GetTimeZoneOffset();
#endif /* !__GLIBC__ */

	return std::chrono::system_clock::from_time_t(t);

#endif /* !WIN32 */
}
