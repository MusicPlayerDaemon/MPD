// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Clock.hxx"
#include "time/FileTime.hxx"

#include <processthreadsapi.h> // for GetProcessTimes()
#include <sysinfoapi.h> // for GetSystemTimeAsFileTime()

std::chrono::seconds
GetProcessUptimeS()
{
	FILETIME creation_time, exit_time, kernel_time, user_time, now;
	GetProcessTimes(GetCurrentProcess(), &creation_time,
			&exit_time, &kernel_time, &user_time);
	GetSystemTimeAsFileTime(&now);

	return DeltaFileTimeS(now, creation_time);
}
