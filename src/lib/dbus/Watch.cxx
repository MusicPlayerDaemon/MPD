/*
 * Copyright 2007-2020 CM4all GmbH
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "Watch.hxx"

#include <cassert>

namespace ODBus {

WatchManager::Watch::Watch(EventLoop &event_loop,
			   WatchManager &_parent, DBusWatch &_watch) noexcept
	:parent(_parent), watch(_watch),
	 event(event_loop, BIND_THIS_METHOD(OnSocketReady))
{
	Toggled();
}

static constexpr unsigned
DbusToLibevent(unsigned flags) noexcept
{
	return ((flags & DBUS_WATCH_READABLE) != 0) * SocketEvent::READ |
		((flags & DBUS_WATCH_WRITABLE) != 0) * SocketEvent::WRITE |
		((flags & DBUS_WATCH_ERROR) != 0) * SocketEvent::ERROR |
		((flags & DBUS_WATCH_HANGUP) != 0) * SocketEvent::HANGUP;
}

void
WatchManager::Watch::Toggled() noexcept
{
	event.ReleaseSocket();

	if (dbus_watch_get_enabled(&watch)) {
		event.Open(SocketDescriptor(dbus_watch_get_unix_fd(&watch)));
		event.Schedule(DbusToLibevent(dbus_watch_get_flags(&watch)));
	}
}

static constexpr unsigned
LibeventToDbus(unsigned flags) noexcept
{
	return ((flags & SocketEvent::READ) != 0) * DBUS_WATCH_READABLE |
		((flags & SocketEvent::WRITE) != 0) * DBUS_WATCH_WRITABLE |
		((flags & SocketEvent::ERROR) != 0) * DBUS_WATCH_ERROR |
		((flags & SocketEvent::HANGUP) != 0) * DBUS_WATCH_HANGUP;
}

void
WatchManager::Watch::OnSocketReady(unsigned events) noexcept
{
	/* copy the "parent" reference to the stack, because the
	   dbus_watch_handle() may invoke WatchManager::Remove() which
	   may destroy this object */
	auto &_parent = parent;

	dbus_watch_handle(&watch, LibeventToDbus(events));

	_parent.ScheduleDispatch();
}

void
WatchManager::Shutdown() noexcept
{
	if (!connection)
		return;

	dbus_connection_set_watch_functions(connection,
					    nullptr, nullptr,
					    nullptr, nullptr,
					    nullptr);
	watches.clear();
	defer_dispatch.Cancel();
}

void
WatchManager::Dispatch() noexcept
{
	while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {}

	if (!dbus_connection_get_is_connected(connection)) {
		Shutdown();
		observer.OnDBusClosed();
	}
}

bool
WatchManager::Add(DBusWatch *watch) noexcept
{
	watches.emplace(std::piecewise_construct,
			std::forward_as_tuple(watch),
			std::forward_as_tuple(GetEventLoop(), *this, *watch));
	return true;
}

void
WatchManager::Remove(DBusWatch *watch) noexcept
{
	watches.erase(watch);
}

void
WatchManager::Toggled(DBusWatch *watch) noexcept
{
	auto i = watches.find(watch);
	assert(i != watches.end());

	i->second.Toggled();
}

} /* namespace ODBus */
