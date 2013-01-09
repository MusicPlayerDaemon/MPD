/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "WakeFD.hxx"
#include "fd_util.h"
#include "gcc.h"

#include <unistd.h>

#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#endif

bool
WakeFD::Create()
{
	assert(fds[0] == -1);
	assert(fds[1] == -1);

#ifdef HAVE_EVENTFD
	fds[0] = eventfd_cloexec_nonblock(0, 0);
	if (fds[0] >= 0) {
		fds[1] = -2;
		return true;
	}
#endif

	return pipe_cloexec_nonblock(fds) >= 0;
}

void
WakeFD::Destroy()
{
#ifndef WIN32
	/* By some strange reason this call hangs on Win32 */
	close(fds[0]);
#endif
#ifdef HAVE_EVENTFD
	if (!IsEventFD())
#endif
		close(fds[1]);

#ifndef NDEBUG
	fds[0] = -1;
	fds[1] = -1;
#endif
}

bool
WakeFD::Read()
{
	assert(fds[0] >= 0);

#ifdef HAVE_EVENTFD
	if (IsEventFD()) {
		eventfd_t value;
		return read(fds[0], &value,
			    sizeof(value)) == (ssize_t)sizeof(value);
	}
#endif

	assert(fds[1] >= 0);

	char buffer[256];
	return read(fds[0], buffer, sizeof(buffer)) > 0;
}

void
WakeFD::Write()
{
	assert(fds[0] >= 0);

#ifdef HAVE_EVENTFD
	if (IsEventFD()) {
		static constexpr eventfd_t value = 1;
		gcc_unused ssize_t nbytes =
			write(fds[0], &value, sizeof(value));
		return;
	}
#endif

	assert(fds[1] >= 0);

	gcc_unused ssize_t nbytes = write(fds[1], "", 1);
}
