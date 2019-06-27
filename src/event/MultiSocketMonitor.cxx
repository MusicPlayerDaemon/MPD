/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "MultiSocketMonitor.hxx"
#include "Loop.hxx"

#include <algorithm>

#ifndef _WIN32
#include <poll.h>
#endif

MultiSocketMonitor::MultiSocketMonitor(EventLoop &_loop) noexcept
	:IdleMonitor(_loop),
	 timeout_event(_loop, BIND_THIS_METHOD(OnTimeout)) {
}

void
MultiSocketMonitor::Reset() noexcept
{
	assert(GetEventLoop().IsInside());

	fds.clear();
	IdleMonitor::Cancel();
	timeout_event.Cancel();
	ready = refresh = false;
}

void
MultiSocketMonitor::ClearSocketList() noexcept
{
	assert(GetEventLoop().IsInside());

	fds.clear();
}

#ifndef _WIN32

void
MultiSocketMonitor::ReplaceSocketList(pollfd *pfds, unsigned n) noexcept
{
	pollfd *const end = pfds + n;

	UpdateSocketList([pfds, end](SocketDescriptor fd) -> unsigned {
			auto i = std::find_if(pfds, end, [fd](const struct pollfd &pfd){
					return pfd.fd == fd.Get();
				});
			if (i == end)
				return 0;

			auto events = i->events;
			i->events = 0;
			return events;
		});

	for (auto i = pfds; i != end; ++i)
		if (i->events != 0)
			AddSocket(SocketDescriptor(i->fd), i->events);
}

#endif

void
MultiSocketMonitor::Prepare() noexcept
{
	const auto timeout = PrepareSockets();
	if (timeout >= timeout.zero())
		timeout_event.Schedule(timeout);
	else
		timeout_event.Cancel();

}

void
MultiSocketMonitor::OnIdle() noexcept
{
	if (ready) {
		ready = false;
		DispatchSockets();

		/* TODO: don't refresh always; require users to call
		   InvalidateSockets() */
		refresh = true;
	}

	if (refresh) {
		refresh = false;
		Prepare();
	}
}
