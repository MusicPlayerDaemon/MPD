// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Convert.hxx"
#include "Zone.hxx"

#include <stdexcept>

#include <time.h>

#ifdef _WIN32
#include <winsock.h>  /* for struct timeval */
#else
#include <sys/time.h>  /* for struct timeval */
#endif

struct tm
GmTime(std::chrono::system_clock::time_point tp)
{
	const time_t t = std::chrono::system_clock::to_time_t(tp);
#ifdef _WIN32
	const struct tm *tm = gmtime(&t);
#else
	struct tm buffer, *tm = gmtime_r(&t, &buffer);
#endif
	if (tm == nullptr)
		throw std::runtime_error("gmtime_r() failed");

	return *tm;
}

struct tm
LocalTime(std::chrono::system_clock::time_point tp)
{
	const time_t t = std::chrono::system_clock::to_time_t(tp);
#ifdef _WIN32
	const struct tm *tm = localtime(&t);
#else
	struct tm buffer, *tm = localtime_r(&t, &buffer);
#endif
	if (tm == nullptr)
		throw std::runtime_error("localtime_r() failed");

	return *tm;
}

std::chrono::system_clock::time_point
TimeGm(struct tm &tm) noexcept
{
#ifdef __GLIBC__
	/* timegm() is a GNU extension */
	const auto t = timegm(&tm);
#else
	tm.tm_isdst = 0;
	const auto t = mktime(&tm) + GetTimeZoneOffset();
#endif /* !__GLIBC__ */

	return std::chrono::system_clock::from_time_t(t);
}

std::chrono::system_clock::time_point
MakeTime(struct tm &tm) noexcept
{
	return std::chrono::system_clock::from_time_t(mktime(&tm));
}

std::chrono::steady_clock::duration
ToSteadyClockDuration(const struct timeval &tv) noexcept
{
	return std::chrono::steady_clock::duration(std::chrono::seconds(tv.tv_sec)) +
		std::chrono::steady_clock::duration(std::chrono::microseconds(tv.tv_usec));
}
