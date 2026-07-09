// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Connection.hxx"
#include "Error.hxx"

namespace ODBus {

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
