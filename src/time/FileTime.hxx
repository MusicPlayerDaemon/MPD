/*
 * Copyright 2013-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef FILE_TIME_HXX
#define FILE_TIME_HXX

#include "SystemClock.hxx"

#include <fileapi.h>

#include <chrono>
#include <cstdint>

constexpr uint_least64_t
ConstructUint64(DWORD lo, DWORD hi) noexcept
{
	return uint_least64_t(lo) | (uint_least64_t(hi) << 32);
}

constexpr uint_least64_t
ToUint64(FILETIME ft) noexcept
{
	return ConstructUint64(ft.dwLowDateTime, ft.dwHighDateTime);
}

constexpr int_least64_t
ToInt64(FILETIME ft) noexcept
{
	return ToUint64(ft);
}

constexpr FILETIME
ToFileTime(uint_least64_t t) noexcept
{
	FILETIME ft{};
	ft.dwLowDateTime = DWORD(t);
	ft.dwHighDateTime = DWORD(t >> 32);
	return ft;
}

constexpr FILETIME
ToFileTime(int_least64_t t) noexcept
{
	return ToFileTime(uint_least64_t(t));
}

/* "A file time is a 64-bit value that represents the number of
   100-nanosecond intervals"
   https://docs.microsoft.com/en-us/windows/win32/sysinfo/file-times */
using FileTimeResolution = std::ratio<1, 10000000>;

using FileTimeDuration = std::chrono::duration<int_least64_t,
					       FileTimeResolution>;

/**
 * Calculate a std::chrono::duration specifying the duration of the
 * FILETIME since its epoch (1601-01-01T00:00).
 */
constexpr auto
FileTimeToChronoDuration(FILETIME ft) noexcept
{
	return FileTimeDuration(ToInt64(ft));
}

/**
 * Calculate a std::chrono::duration specifying the duration between
 * the unix epoch and the given FILETIME.
 */
constexpr auto
FileTimeToUnixEpochDuration(FILETIME ft) noexcept
{
	/**
	 * The number of days between the Windows FILETIME epoch
	 * (1601-01-01T00:00) and the Unix epoch (1970-01-01T00:00).
	 */
	constexpr int_least64_t windows_unix_days = 134774;
	constexpr int_least64_t windows_unix_hours = windows_unix_days * 24;

	constexpr FileTimeDuration windows_unix_delta{std::chrono::hours{windows_unix_hours}};

	return FileTimeToChronoDuration(ft) - windows_unix_delta;
}

inline std::chrono::system_clock::time_point
FileTimeToChrono(FILETIME ft) noexcept
{
	return TimePointAfterUnixEpoch(FileTimeToUnixEpochDuration(ft));
}

constexpr FILETIME
ToFileTime(FileTimeDuration d) noexcept
{
	return ToFileTime(d.count());
}

constexpr FILETIME
UnixEpochDurationToFileTime(FileTimeDuration d) noexcept
{
	/**
	 * The number of days between the Windows FILETIME epoch
	 * (1601-01-01T00:00) and the Unix epoch (1970-01-01T00:00).
	 */
	constexpr int_least64_t windows_unix_days = 134774;
	constexpr int_least64_t windows_unix_hours = windows_unix_days * 24;

	constexpr FileTimeDuration windows_unix_delta{std::chrono::hours{windows_unix_hours}};

	return ToFileTime(d + windows_unix_delta);
}

inline FILETIME
ChronoToFileTime(std::chrono::system_clock::time_point tp) noexcept
{
	const auto since_unix_epoch = DurationSinceUnixEpoch(tp);
	const auto ft_since_unix_epoch =
		std::chrono::duration_cast<FileTimeDuration>(since_unix_epoch);

	return UnixEpochDurationToFileTime(ft_since_unix_epoch);
}

constexpr std::chrono::seconds
DeltaFileTimeS(FILETIME a, FILETIME b) noexcept
{
	return std::chrono::duration_cast<std::chrono::seconds>
		(FileTimeToChronoDuration(a) - FileTimeToChronoDuration(b));
}

#endif
