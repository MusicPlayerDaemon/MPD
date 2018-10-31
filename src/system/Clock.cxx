/*
 * Copyright 2003-2018 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Clock.hxx"

#ifdef _WIN32
#include <windows.h>

gcc_const
static unsigned
DeltaFileTimeS(FILETIME a, FILETIME b)
{
	ULARGE_INTEGER a2, b2;
	b2.LowPart = b.dwLowDateTime;
	b2.HighPart = b.dwHighDateTime;
	a2.LowPart = a.dwLowDateTime;
	a2.HighPart = a.dwHighDateTime;
	return (a2.QuadPart - b2.QuadPart) / 10000000;
}

unsigned
GetProcessUptimeS()
{
	FILETIME creation_time, exit_time, kernel_time, user_time, now;
	GetProcessTimes(GetCurrentProcess(), &creation_time,
			&exit_time, &kernel_time, &user_time);
	GetSystemTimeAsFileTime(&now);

	return DeltaFileTimeS(now, creation_time);
}

#endif
