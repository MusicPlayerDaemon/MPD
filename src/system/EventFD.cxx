/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "config.h"
#ifdef USE_EVENTFD
#include "EventFD.hxx"
#include "system/FatalError.hxx"
#include "Compiler.h"

#include <assert.h>
#include <sys/eventfd.h>

EventFD::EventFD()
{
	if (!fd.CreateEventFD(0))
		FatalSystemError("eventfd() failed");
}

bool
EventFD::Read()
{
	assert(fd.IsDefined());

	eventfd_t value;
	return fd.Read(&value, sizeof(value)) == (ssize_t)sizeof(value);
}

void
EventFD::Write()
{
	assert(fd.IsDefined());

	static constexpr eventfd_t value = 1;
	gcc_unused ssize_t nbytes =
		fd.Write(&value, sizeof(value));
}

#endif /* USE_EVENTFD */
