// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "Connection.hxx"
#include "Error.hxx"

ODBus::Connection
ODBus::Connection::GetSystem()
{
	ODBus::Error error;
	auto *c = dbus_bus_get(DBUS_BUS_SYSTEM, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}

ODBus::Connection
ODBus::Connection::GetSystemPrivate()
{
	ODBus::Error error;
	auto *c = dbus_bus_get_private(DBUS_BUS_SYSTEM, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}

ODBus::Connection
ODBus::Connection::Open(const char *address)
{
	ODBus::Error error;
	auto *c = dbus_connection_open(address, error);
	error.CheckThrow("DBus connection error");
	return Connection(c);
}
