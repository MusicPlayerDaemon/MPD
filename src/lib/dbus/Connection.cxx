// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Connection.hxx"
#include "Error.hxx"

#include <dbus/dbus.h>

namespace ODBus {

Connection::Connection(const Connection &src) noexcept
	:c(dbus_connection_ref(src.c)) {}

void
Connection::Close() noexcept
{
	dbus_connection_close(c);
}

void
Connection::Unref(DBusConnection *c) noexcept
{
	dbus_connection_unref(c);
}

Connection
Connection::GetSystem()
{
	Error error;
	auto *c = dbus_bus_get(DBUS_BUS_SYSTEM, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}

Connection
Connection::GetSystemPrivate()
{
	Error error;
	auto *c = dbus_bus_get_private(DBUS_BUS_SYSTEM, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}

Connection
Connection::Open(const char *address)
{
	Error error;
	auto *c = dbus_connection_open(address, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}

} // namespace ODBus
