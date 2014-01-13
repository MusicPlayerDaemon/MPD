/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#ifdef USE_SIGNALFD
#include "SignalFD.hxx"
#include "FatalError.hxx"

#include <assert.h>
#include <unistd.h>
#include <sys/signalfd.h>

void
SignalFD::Create(const sigset_t &mask)
{
	fd = ::signalfd(fd, &mask, SFD_NONBLOCK|SFD_CLOEXEC);
	if (fd < 0)
		FatalSystemError("signalfd() failed");
}

void
SignalFD::Close()
{
	if (fd >= 0) {
		::close(fd);
		fd = -1;
	}
}

int
SignalFD::Read()
{
	assert(fd >= 0);

	signalfd_siginfo info;
	return read(fd, &info, sizeof(info)) > 0
		? info.ssi_signo
		: -1;
}

#endif /* USE_SIGNALFD */
