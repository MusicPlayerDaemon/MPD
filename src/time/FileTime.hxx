/*
 * Copyright 2013-2019 Max Kellermann <max.kellermann@gmail.com>
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

constexpr time_t
FileTimeToTimeT(FILETIME ft) noexcept
{
	return (ToInt64(ft) - 116444736000000000) / 10000000;
}

inline std::chrono::system_clock::time_point
FileTimeToChrono(FILETIME ft) noexcept
{
	// TODO: eliminate the time_t roundtrip, preserve sub-second resolution
	return std::chrono::system_clock::from_time_t(FileTimeToTimeT(ft));
}

constexpr std::chrono::seconds
DeltaFileTimeS(FILETIME a, FILETIME b) noexcept
{
	return std::chrono::duration_cast<std::chrono::seconds>
		(FileTimeToChronoDuration(a) - FileTimeToChronoDuration(b));
}

#endif
