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
#include "SocketMonitor.hxx"
#include "Loop.hxx"
#include "system/fd_util.h"
#include "Compiler.h"

#include <assert.h>

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifdef USE_EPOLL

void
SocketMonitor::Dispatch(unsigned flags)
{
	flags &= GetScheduledFlags();

	if (flags != 0 && !OnSocketReady(flags) && IsDefined())
		Cancel();
}

#else

/*
 * GSource methods
 *
 */

gboolean
SocketMonitor::Prepare(gcc_unused GSource *source, gcc_unused gint *timeout_r)
{
	return false;
}

gboolean
SocketMonitor::Check(GSource *_source)
{
	const Source &source = *(const Source *)_source;
	const SocketMonitor &monitor = *source.monitor;
	assert(_source == &monitor.source->base);

	return monitor.Check();
}

gboolean
SocketMonitor::Dispatch(GSource *_source,
			gcc_unused GSourceFunc callback,
			gcc_unused gpointer user_data)
{
	Source &source = *(Source *)_source;
	SocketMonitor &monitor = *source.monitor;
	assert(_source == &monitor.source->base);

	monitor.Dispatch();
	return true;
}

/**
 * The vtable for our GSource implementation.  Unfortunately, we
 * cannot declare it "const", because g_source_new() takes a non-const
 * pointer, for whatever reason.
 */
static GSourceFuncs socket_monitor_source_funcs = {
	SocketMonitor::Prepare,
	SocketMonitor::Check,
	SocketMonitor::Dispatch,
	nullptr,
	nullptr,
	nullptr,
};

SocketMonitor::SocketMonitor(int _fd, EventLoop &_loop)
	:fd(-1), loop(_loop),
	 source(nullptr) {
	assert(_fd >= 0);

	Open(_fd);
}

#endif

SocketMonitor::~SocketMonitor()
{
	if (IsDefined())
		Close();
}

void
SocketMonitor::Open(int _fd)
{
	assert(fd < 0);
#ifndef USE_EPOLL
	assert(source == nullptr);
#endif
	assert(_fd >= 0);

	fd = _fd;

#ifndef USE_EPOLL
	poll = {fd, 0, 0};

	source = (Source *)g_source_new(&socket_monitor_source_funcs,
					sizeof(*source));
	source->monitor = this;

	g_source_attach(&source->base, loop.GetContext());
	g_source_add_poll(&source->base, &poll);
#endif
}

int
SocketMonitor::Steal()
{
	assert(IsDefined());

	Cancel();

	int result = fd;
	fd = -1;

#ifndef USE_EPOLL
	g_source_destroy(&source->base);
	g_source_unref(&source->base);
	source = nullptr;
#endif

	return result;
}

void
SocketMonitor::Abandon()
{
	assert(IsDefined());

#ifdef USE_EPOLL
	fd = -1;
	loop.Abandon(*this);
#else
	Steal();
#endif
}

void
SocketMonitor::Close()
{
	close_socket(Steal());
}

void
SocketMonitor::Schedule(unsigned flags)
{
	assert(IsDefined());

	if (flags == GetScheduledFlags())
		return;

#ifdef USE_EPOLL
	if (scheduled_flags == 0)
		loop.AddFD(fd, flags, *this);
	else if (flags == 0)
		loop.RemoveFD(fd, *this);
	else
		loop.ModifyFD(fd, flags, *this);

	scheduled_flags = flags;
#else
	poll.events = flags;
	poll.revents &= flags;

	loop.WakeUp();
#endif
}

SocketMonitor::ssize_t
SocketMonitor::Read(void *data, size_t length)
{
	assert(IsDefined());

	int flags = 0;
#ifdef MSG_DONTWAIT
	flags |= MSG_DONTWAIT;
#endif

	return recv(Get(), (char *)data, length, flags);
}

SocketMonitor::ssize_t
SocketMonitor::Write(const void *data, size_t length)
{
	assert(IsDefined());

	int flags = 0;
#ifdef MSG_NOSIGNAL
	flags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_DONTWAIT
	flags |= MSG_DONTWAIT;
#endif

	return send(Get(), (const char *)data, length, flags);
}
